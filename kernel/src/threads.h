#pragma once

#include "capabilities.h"
#include "arch.h"
#include "linked_list.h"

#define THREAD_CURRENTLY_RUNNING 1
#define THREAD_NEEDS_CPU_UPDATE 2

struct thread_capability {
    /// the root capability of this thread, all of its capability addresses are relative to this
    struct capability root_capability;
    /// the execution mode of this thread
    enum {
        RUNNING,
        BLOCKED,
        SUSPENDED,
        EXITED
    } exec_mode;
    /// niceness value of this thread (-20 to 20)
    int8_t niceness;
    /// estimate of how much CPU time this thread has used recently in 17.14 fixed point
    uint32_t recent_cpu_time;
    /// the saved registers of this thread
    struct thread_registers registers;
    /// flags used by the scheduler
    uint8_t scheduler_flags;
    /// the priority of this thread (which priority queue it's in)
    uint8_t priority;
    /// a reference to the start and end of the runqueue that this thread is in
    LIST_CONTAINER(struct thread_capability) *runqueue;
    /// contains the previous and next links in the runqueue
    LIST_LINK(struct thread_capability) runqueue_entry;
    /// contains the previous and next links in the cpu update list
    LIST_LINK(struct thread_capability) cpu_update_entry;
    /// a unique id number that's assigned to each thread. once a thread's capability has been freed, its id can be reassigned to a newly created thread
    uint16_t thread_id;
    /// the bucket in the thread hash table that this thread is placed in
    uint8_t bucket_number;
    /// linked list that forms each bucket of the thread hash table
    LIST_LINK(struct thread_capability) table_entry;
};

extern struct invocation_handlers thread_handlers;

void init_threads(void);
void on_thread_moved(struct thread_capability *thread);
bool look_up_thread_by_id(uint16_t thread_id, uint8_t bucket_number, struct thread_capability **thread);

#include "heap.h"

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers);
