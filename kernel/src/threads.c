#include "threads.h"
#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#include "capabilities.h"
#include "heap.h"
#include "string.h"

extern struct invocation_handlers untyped_handlers;

static size_t read_registers(struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy(args->address, (const void *) &thread->registers, args->size > sizeof(struct registers) ? sizeof(struct registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t write_registers(struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy((void *) &thread->registers, args->address, args->size > sizeof(struct registers) ? sizeof(struct registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static struct invocation_handlers thread_handlers = {
    2,
    {read_registers, write_registers}
};

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers) {
    struct thread_capability *thread = heap_alloc(heap, sizeof(struct thread_capability));

    memset((char *) thread, 0, sizeof(struct thread_capability));
    thread->exec_mode = EXITED;

    *resource = (void *) thread;
    *handlers = &thread_handlers;
}
