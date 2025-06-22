#include "main.h"
#include "bflt.h"
#include "capabilities.h"
#include "debug.h"
#include "heap.h"
#include "scheduler.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "threads.h"

extern uint8_t _binary_process_server_start[];
extern uint8_t _binary_process_server_end;

struct thread_capability root_thread;

void main_init(struct heap *heap) {
    printk("Hellorld!\n");

    /*printk("sizeof(struct capability) is %d\n", sizeof(struct capability));
    printk("sizeof(struct heap_header) is %d\n", sizeof(struct heap_header));*/

    init_threads();
    init_scheduler();

    struct thread_registers registers;

    size_t process_server_size = (size_t) &_binary_process_server_end - (size_t) &_binary_process_server_start;
    printk("process server is at 0x%x to 0x%x (%d bytes)\n", &_binary_process_server_start, &_binary_process_server_end, process_server_size);

    struct bflt_header *header = (struct bflt_header *) &_binary_process_server_start;

    if (!bflt_verify(header)) {
        printk("format of process server binary is invalid, cannot continue\n");
        return;
    }

    size_t allocation_size = bflt_allocation_size(header);
    printk("allocation size for process server is %d\n", allocation_size);

    void *allocation = heap_alloc(heap, allocation_size);

    if (allocation == NULL) {
        printk("couldn't allocate memory for process server's data and/or code\n");
        return;
    }

    printk("process server's allocated data is at 0x%x to 0x%x\n", allocation, (size_t) allocation + allocation_size);

    bflt_load(header, allocation, &registers);

    struct thread_capability *thread = alloc_thread(heap);

    if (thread == NULL) {
        printk("process server's thread allocation failed! cannot continue\n");
        return;
    }

    memcpy(&thread->registers, &registers, sizeof(struct thread_registers));

    // this'll get the thread to start running once context switching is started in the platform layer
    resume_thread(thread, EXEC_MODE_SUSPENDED);

    heap_set_update_function(thread, on_thread_moved);

    // zero out the root capability slot to make initializing easier
    memset(&thread->root_capability, 0, sizeof(struct capability));

    // allocate the root node resource
    thread->root_capability.handlers = &node_handlers;
    thread->root_capability.resource = alloc_node(heap, ROOT_CAP_SLOT_BITS);

    if (thread->root_capability.resource == NULL) {
        printk("allocation of process server's root capability node failed, cannot continue\n");
        return;
    }

    thread->root_capability.flags = CAP_FLAG_IS_HEAP_MANAGED | CAP_FLAG_ORIGINAL;

    thread->root_capability.access_rights = UINT8_MAX; // all rights given

    LIST_INIT_NO_CONTAINER(&thread->root_capability, resource_list);

    thread->root_capability.heap = heap;

    thread->root_capability.address.thread_id = thread->thread_id;
    thread->root_capability.address.bucket_number = thread->bucket_number;
    // everything else here assumes NULL is 0

    heap_set_update_capability(thread->root_capability.resource, &thread->root_capability.address);
    heap_unlock(thread->root_capability.resource);

    // set the current thread so that capability lookups work properly while the init thread's capability space is being set up
    thread->flags |= THREAD_CURRENTLY_RUNNING;
    scheduler_state.current_thread = thread;

    heap_unlock(thread);

    struct address_space_capability *address_space_resource = heap_alloc(heap, sizeof(struct address_space_capability));
    address_space_resource->heap_pointer = heap;
    populate_capability_slot(heap, 0, ROOT_CAP_SLOT_BITS, address_space_resource, &address_space_handlers, CAP_FLAG_IS_HEAP_MANAGED);

    // add debug capability to thread's root node
    populate_capability_slot(heap, 1, ROOT_CAP_SLOT_BITS, NULL, &debug_handlers, 0);

    /*struct look_up_result result;
    look_up_capability_relative(2, ROOT_CAP_SLOT_BITS, &result);
    copy_capability(&scheduler_state.current_thread->root_capability, result.slot, 2, ROOT_CAP_SLOT_BITS);
    unlock_looked_up_capability(&result);*/

    // reset the current thread to NULL since it's not running yet
    scheduler_state.current_thread->flags &= (uint8_t) ~THREAD_CURRENTLY_RUNNING;
    scheduler_state.current_thread = NULL;

    printk(
        "total memory: %d KiB, used memory: %d KiB, free memory: %d KiB\n",
        heap->total_memory / 1024,
        heap->used_memory / 1024,
        (heap->total_memory - heap->used_memory) / 1024
    );
}
