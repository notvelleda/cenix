#include "threads.h"
#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#include "capabilities.h"
#include "heap.h"
#include "string.h"
#include "scheduler.h"
#include "sys/kernel.h"
#include "linked_list.h"

#undef DEBUG_THREADS

#define NUM_BUCKETS 128

static LIST_CONTAINER(struct thread_capability) thread_hash_table[NUM_BUCKETS];

#define MAX_THREADS 1024

#define PTR_BITS (sizeof(size_t) * 8)
#define USED_THREAD_IDS_SIZE (MAX_THREADS / PTR_BITS)
static size_t used_thread_ids[USED_THREAD_IDS_SIZE];

// 32-bit fnv-1a hash, as described in http://isthe.com/chongo/tech/comp/fnv/
static uint32_t hash(uint16_t value) {
    const uint8_t *data = (uint8_t *) &value;
    uint32_t result = 0x811c9dc5;
    result ^= data[0];
    result *= 0x01000193;
    result ^= data[1];
    result *= 0x01000193;
    return result;
}

void init_threads(void) {
    for (int i = 0; i < NUM_BUCKETS; i ++) {
        LIST_INIT(thread_hash_table[i]);
    }

    for (int i = 0; i < USED_THREAD_IDS_SIZE; i ++) {
        used_thread_ids[i] = 0;
    }
    used_thread_ids[0] = 1; // id 0 is reserved for kernel resources
}

extern struct invocation_handlers untyped_handlers;

static size_t read_registers(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy(args->address, (const void *) &thread->registers, args->size > sizeof(struct thread_registers) ? sizeof(struct thread_registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t write_registers(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    const struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy(&thread->registers, args->address, args->size > sizeof(struct thread_registers) ? sizeof(struct thread_registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t resume(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    resume_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t suspend(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    suspend_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t set_root_node(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    //bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;
    const struct set_root_node_args *args = (struct set_root_node_args *) argument;
    struct look_up_result result;

    if (!look_up_capability_relative(args->address, args->depth, from_userland, &result)) {
        /*if (should_unlock) {
            heap_unlock(slot->resource);
        }*/

        return 0;
    }

    size_t return_value = 0;

    if (result.slot->handlers == &node_handlers && get_nested_nodes_depth(result.slot) < MAX_NESTED_NODES) {
        return_value = 1;

        struct capability *root_slot = &thread->root_capability;
        move_capability(result.slot, root_slot);
        update_capability_addresses(root_slot, 0, 0, thread->thread_id, thread->bucket_number, 0);
    }

    unlock_looked_up_capability(&result);

    /*if (should_unlock) {
        heap_unlock(slot->resource);
    }*/

    return return_value;
}

static void thread_destructor(struct capability *slot) {
    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    delete_capability(&thread->root_capability);

    if (thread->runqueue != NULL) {
        LIST_REMOVE(*thread->runqueue, runqueue_entry, thread);
    }

    if ((thread->scheduler_flags & THREAD_NEEDS_CPU_UPDATE) != 0) {
        LIST_REMOVE(scheduler_state.needs_cpu_time_update, cpu_update_entry, thread);
    }

    LIST_REMOVE(thread_hash_table[thread->bucket_number], table_entry, thread);

    used_thread_ids[thread->thread_id / PTR_BITS] &= ~(1 << (thread->thread_id % PTR_BITS)); // release thread id
}

struct invocation_handlers thread_handlers = {
    5,
    {read_registers, write_registers, resume, suspend, set_root_node},
    thread_destructor
};

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers) {
    uint16_t thread_id = 0;
    bool has_thread_id = false;

    // allocate a new thread id for this thread
    for (int i = 0; i < USED_THREAD_IDS_SIZE; i ++) {
        if (used_thread_ids[i] == SIZE_MAX) {
            continue;
        }

        int bit;
        for (bit = 0; bit < PTR_BITS && (used_thread_ids[i] & (1 << bit)) != 0; bit ++);

        used_thread_ids[i] |= (1 << bit);

        thread_id = i * PTR_BITS + bit;
        has_thread_id = true;
        break;
    }

    // if a thread id couldn't be found, give up
    if (!has_thread_id) {
#ifdef DEBUG_THREADS
        printk("threads: couldn't allocate id for new thread!\n");
#endif
        *resource = NULL;
        *handlers = NULL;
        return;
    }

#ifdef DEBUG_THREADS
    printk("threads: allocated id %d for new thread\n", thread_id);
#endif

    struct thread_capability *thread = heap_alloc(heap, sizeof(struct thread_capability));

    if (thread == NULL) {
#ifdef DEBUG_THREADS
        printk("threads: heap_alloc for new thread failed!\n");
#endif
        used_thread_ids[thread_id / PTR_BITS] &= ~(1 << (thread_id % PTR_BITS)); // release thread id
        *resource = NULL;
        *handlers = NULL;
        return;
    }

    memset((uint8_t *) thread, 0, sizeof(struct thread_capability));
    thread->exec_mode = SUSPENDED;
    thread->thread_id = thread_id;
    // queue entries don't need to be set to NULL here as long as NULL is 0

    uint32_t id_hash = hash(thread_id);
    thread->bucket_number = id_hash % NUM_BUCKETS;

#ifdef DEBUG_THREADS
    printk("threads: hashed thread id is 0x%08x, bucket number is %d\n", id_hash, thread->bucket_number);
#endif

    // insert this thread into the thread hash table
    LIST_APPEND(thread_hash_table[thread->bucket_number], table_entry, thread);

    *resource = thread;
    *handlers = &thread_handlers;
}

void on_thread_moved(struct thread_capability *thread) {
    if (thread->runqueue != NULL) {
        // update references to this thread in its current runqueue
        LIST_UPDATE_ADDRESS(*thread->runqueue, runqueue_entry, thread);
    }

    // update references to this thread in the thread hash table
    LIST_UPDATE_ADDRESS(thread_hash_table[thread->bucket_number], table_entry, thread);

    if ((thread->scheduler_flags & THREAD_NEEDS_CPU_UPDATE) != 0) {
        // update references to this thread in the needs cpu update queue
        LIST_UPDATE_ADDRESS(scheduler_state.needs_cpu_time_update, cpu_update_entry, thread);
    }

    if ((thread->scheduler_flags & THREAD_CURRENTLY_RUNNING) != 0) {
        scheduler_state.current_thread = thread;
    }
}

bool look_up_thread_by_id(uint16_t thread_id, uint8_t bucket_number, struct thread_capability **thread) {
    *thread = NULL;
    return false;
}
