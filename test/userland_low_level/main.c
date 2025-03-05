#include <assert.h>
#include "debug.h"
#include <errno.h>
#include "heap.h"
#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include "threads.h"
#include "unity.h"

// weak linked stubs that programs under test can override

__attribute__((weak)) size_t read_registers(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) size_t write_registers(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) size_t resume(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) size_t suspend(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) size_t set_root_node(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) void thread_destructor(struct capability *slot) {}

__attribute__((weak)) size_t endpoint_send(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) size_t endpoint_receive(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return (size_t) ENOSYS;
}

__attribute__((weak)) void endpoint_destructor(struct capability *slot) {}

__attribute__((weak)) void syscall_yield(void) {}

__attribute__((weak)) void custom_setup(void) {}
__attribute__((weak)) void custom_teardown(void) {}

struct invocation_handlers thread_handlers = {
    .num_handlers = 5,
    .handlers = {read_registers, write_registers, resume, suspend, set_root_node},
    .destructor = thread_destructor
};

struct invocation_handlers endpoint_handlers = {
    .num_handlers = 2,
    .handlers = {endpoint_send, endpoint_receive},
    .destructor = endpoint_destructor
};

struct scheduler_state scheduler_state;

size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument) {
    return invoke_capability(address, depth, handler_number, argument);
}

size_t read_badge(size_t address, size_t depth, size_t *badge) {
    struct look_up_result result;

    if (!look_up_capability_relative(address, depth, &result)) {
        return ENOCAPABILITY;
    }

    *badge = result.slot->badge;
    return 0;
}

/// called by unity before each test runs, used to set up the capability space for the program under test
void setUp(void) {
    struct thread_capability *thread = heap_alloc(NULL, sizeof(struct thread_capability));
    assert(thread != NULL);

    thread->thread_id = 0;
    thread->bucket_number = 0;

    // mostly copy-pasted from core/kernel/main.c

    // zero out the root capability slot to make initializing easier
    memset(&thread->root_capability, 0, sizeof(struct capability));

    // allocate the root node resource
    thread->root_capability.handlers = &node_handlers;
    thread->root_capability.resource = alloc_node(NULL, ROOT_CAP_SLOT_BITS);

    assert(thread->root_capability.resource != NULL);

    thread->root_capability.flags = CAP_FLAG_IS_HEAP_MANAGED | CAP_FLAG_ORIGINAL;

    thread->root_capability.access_rights = -1; // all rights given

    LIST_INIT_NO_CONTAINER(&thread->root_capability, resource_list);

    thread->root_capability.heap = NULL;

    thread->root_capability.address.thread_id = thread->thread_id;
    thread->root_capability.address.bucket_number = thread->bucket_number;
    // everything else here assumes NULL is 0

    heap_set_update_capability(thread->root_capability.resource, &thread->root_capability.address);
    heap_unlock(thread->root_capability.resource);

    // set the current thread so that capability lookups work properly while the thread's capability space is being set up
    scheduler_state.current_thread = thread;

    heap_unlock(thread);

    struct address_space_capability *address_space_resource = heap_alloc(NULL, sizeof(struct address_space_capability));
    address_space_resource->heap_pointer = NULL;
    populate_capability_slot(NULL, 0, ROOT_CAP_SLOT_BITS, address_space_resource, &address_space_handlers, CAP_FLAG_IS_HEAP_MANAGED);

    // add debug capability to thread's root node
    populate_capability_slot(NULL, 1, ROOT_CAP_SLOT_BITS, NULL, &debug_handlers, 0);

    // call the custom setup function to allow programs under test to set up their environments further
    custom_setup();
}

/// called by unity after each test runs, used to free all memory allocated for capabilities used by the program under test
void tearDown(void) {
    custom_teardown();
    delete_capability(&scheduler_state.current_thread->root_capability);
    heap_free(NULL, scheduler_state.current_thread);
}
