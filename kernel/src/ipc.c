#include "ipc.h"
#include "capabilities.h"
#include "heap.h"
#include "linked_list.h"
#include "scheduler.h"
#include "string.h"
#include "sys/kernel.h"
#include "threads.h"

static void transfer_capabilities(
    const struct thread_capability *sending,
    const struct thread_capability *receiving,
    struct ipc_message *sent_message,
    struct ipc_message *recv_buffer
) {
    recv_buffer->transferred_capabilities = 0;

    for (int i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
        if (sent_message->capabilities[i].depth == 0 || recv_buffer->capabilities[i].depth == 0) {
            continue;
        }

        struct absolute_capability_address source_address;
        source_address.thread_id = sending->thread_id;
        source_address.bucket_number = sending->bucket_number;
        source_address.address = sent_message->capabilities[i].address;
        source_address.depth = sent_message->capabilities[i].depth;

        struct look_up_result source_result;

        if (!look_up_capability_absolute(&source_address, &source_result)) {
            continue;
        }

        if (source_result.slot->handlers == NULL) {
            unlock_looked_up_capability(&source_result);
            continue;
        }

        struct absolute_capability_address dest_address;
        dest_address.thread_id = receiving->thread_id;
        dest_address.bucket_number = receiving->bucket_number;
        dest_address.address = recv_buffer->capabilities[i].address;
        dest_address.depth = recv_buffer->capabilities[i].depth;

        struct look_up_result dest_result;

        if (!look_up_capability_absolute(&dest_address, &dest_result)) {
            unlock_looked_up_capability(&source_result);
            continue;
        }

        if (dest_result.slot->handlers == NULL) {
            unlock_looked_up_capability(&source_result);
            unlock_looked_up_capability(&dest_result);
            continue;
        }

        move_capability(source_result.slot, dest_result.slot);
        update_capability_addresses(dest_result.slot, &dest_address, 0);
        recv_buffer->transferred_capabilities |= 1 << i;

        unlock_looked_up_capability(&source_result);
        unlock_looked_up_capability(&dest_result);
    }
}

static size_t endpoint_send(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;
    struct endpoint_capability *endpoint = (struct endpoint_capability *) slot->resource;

    if (LIST_CAN_POP(endpoint->blocked_receiving)) {
        // there's already a thread waiting to receive the message
        struct thread_capability *receiving;
        LIST_POP_FROM_START(endpoint->blocked_receiving, blocked_queue, receiving);
        receiving->flags &= ~THREAD_BLOCKED_ON_RECEIVE;

        memcpy(&receiving->message_buffer->buffer, &message->buffer, IPC_BUFFER_SIZE);
        receiving->message_buffer->badge = slot->badge;

        transfer_capabilities(scheduler_state.current_thread, receiving, message, receiving->message_buffer);

        // TODO: is it worth just switching directly to the receiving thread? would that be faster?
        resume_thread(receiving, EXEC_MODE_BLOCKED);
    } else {
        // calling thread has to be blocked until a thread tries to receive the message
        struct thread_capability *thread = scheduler_state.current_thread;

        thread->message_buffer = message;
        thread->sending_badge = slot->badge;
        thread->blocked_on = endpoint;
        thread->flags |= THREAD_BLOCKED_ON_SEND;

        LIST_APPEND(endpoint->blocked_sending, blocked_queue, thread);

        suspend_thread(thread, EXEC_MODE_BLOCKED);
    }

    return 0;
}

static size_t endpoint_receive(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;
    struct endpoint_capability *endpoint = (struct endpoint_capability *) slot->resource;

    if (LIST_CAN_POP(endpoint->blocked_sending)) {
        // there's already a thread waiting to send a message
        struct thread_capability *sending;
        LIST_POP_FROM_START(endpoint->blocked_sending, blocked_queue, sending);
        sending->flags &= ~THREAD_BLOCKED_ON_SEND;

        memcpy(&message->buffer, &sending->message_buffer->buffer, IPC_BUFFER_SIZE);
        message->badge = sending->sending_badge;

        transfer_capabilities(sending, scheduler_state.current_thread, sending->message_buffer, message);

        // TODO: is it worth just switching directly to the sending thread? would that be faster?
        resume_thread(sending, EXEC_MODE_BLOCKED);
    } else {
        // calling thread has to be blocked until a thread tries to send a message
        struct thread_capability *thread = scheduler_state.current_thread;

        thread->message_buffer = message;
        thread->sending_badge = slot->badge;
        thread->blocked_on = endpoint;
        thread->flags |= THREAD_BLOCKED_ON_RECEIVE;

        LIST_APPEND(endpoint->blocked_receiving, blocked_queue, thread);

        suspend_thread(thread, EXEC_MODE_BLOCKED);
    }

    return 0;
}

static void on_endpoint_moved(void *resource) {
    struct endpoint_capability *endpoint = (struct endpoint_capability *) resource;

    LIST_ITER(struct thread_capability, endpoint->blocked_sending, blocked_queue, thread) {
        thread->blocked_on = endpoint;
    }

    LIST_ITER(struct thread_capability, endpoint->blocked_receiving, blocked_queue, thread) {
        thread->blocked_on = endpoint;
    }
}

static void endpoint_destructor(struct capability *slot) {
    const struct endpoint_capability *endpoint = (struct endpoint_capability *) slot->resource;

    LIST_ITER(struct thread_capability, endpoint->blocked_sending, blocked_queue, thread) {
        suspend_thread(thread, EXEC_MODE_SUSPENDED);
        resume_thread(thread, EXEC_MODE_BLOCKED);
        thread->blocked_on = NULL;
    }

    LIST_ITER(struct thread_capability, endpoint->blocked_receiving, blocked_queue, thread) {
        suspend_thread(thread, EXEC_MODE_SUSPENDED);
        resume_thread(thread, EXEC_MODE_BLOCKED);
        thread->blocked_on = NULL;
    }
}

struct invocation_handlers endpoint_handlers = {
    .num_handlers = 2,
    .handlers = {endpoint_send, endpoint_receive},
    .on_moved = on_endpoint_moved,
    .destructor = endpoint_destructor
};

struct endpoint_capability *alloc_endpoint(struct heap *heap) {
    struct endpoint_capability *endpoint = (struct endpoint_capability *) heap_alloc(heap, sizeof(struct endpoint_capability));

    if (endpoint == NULL) {
        return NULL;
    }

    LIST_INIT(endpoint->blocked_sending);
    LIST_INIT(endpoint->blocked_receiving);

    return endpoint;
}
