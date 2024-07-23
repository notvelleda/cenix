#pragma once

#include "capabilities.h"
#include "arch.h"

struct thread_capability {
    // the root capability of this thread, all of its capability addresses are relative to this
    struct capability root_capability;
    // the execution mode of this thread
    enum {
        RUNNING,
        BLOCKED,
        EXITED
    } exec_mode;
    // niceness value of this thread (-20 to 20)
    int8_t niceness;
    // estimate of how much CPU time this thread has used recently in 17.14 fixed point
    uint32_t cpu_time;
    // the saved registers of this thread
    struct registers registers;
};

#include "heap.h"

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers);

#define THREAD_READ_REGISTERS 0
#define THREAD_WRITE_REGISTERS 1

struct read_write_register_args {
    void *address;
    size_t size;
};
