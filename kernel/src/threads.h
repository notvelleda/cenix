#pragma once

#include "capabilities.h"
#include "arch.h"

struct thread_queue {
    struct thread_capability *start;
    struct thread_capability *end;
};

struct thread_queue_entry {
    struct thread_capability *next;
    struct thread_capability *prev;
};

#define THREAD_CURRENTLY_RUNNING 1
#define THREAD_NEEDS_CPU_UPDATE 2

struct thread_capability {
    // the root capability of this thread, all of its capability addresses are relative to this
    struct capability root_capability;
    // the execution mode of this thread
    enum {
        RUNNING,
        BLOCKED,
        SUSPENDED,
        EXITED
    } exec_mode;
    // niceness value of this thread (-20 to 20)
    int8_t niceness;
    // estimate of how much CPU time this thread has used recently in 17.14 fixed point
    uint32_t recent_cpu_time;
    // the saved registers of this thread
    struct registers registers;
    // flags used by the scheduler
    uint8_t scheduler_flags;
    // the priority of this thread (which priority queue it's in)
    uint8_t priority;
    // linked list that forms each scheduler queue
    struct thread_queue *runqueue;
    struct thread_queue_entry runqueue_entry;
    struct thread_queue_entry cpu_update_entry;
    // a unique id number that's assigned to each thread. once a thread's capability has been freed, its id can be reassigned to a newly created thread
    uint16_t thread_id;
    // the hashed version of thread_id, used to index a bucket in the thread hash table
    uint32_t id_hash;
    // linked list that forms each bucket of the thread hash table
    struct thread_queue_entry table_entry;
};

extern struct invocation_handlers thread_handlers;

void init_threads(void);
void on_thread_moved(struct thread_capability *thread);

#include "heap.h"

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers);

#define THREAD_READ_REGISTERS 0
#define THREAD_WRITE_REGISTERS 1
#define THREAD_RESUME 2
#define THREAD_SUSPEND 3

struct read_write_register_args {
    void *address;
    size_t size;
};
