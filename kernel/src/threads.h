#pragma once

#include "capabilities.h"
#include "arch.h"
#include "linked_list.h"
#include "ipc.h"

#define THREAD_CURRENTLY_RUNNING 1
#define THREAD_NEEDS_CPU_UPDATE 2
#define THREAD_BLOCKED_ON_SEND 4
#define THREAD_BLOCKED_ON_RECEIVE 8

#define EXEC_MODE_RUNNING 0
#define EXEC_MODE_BLOCKED 1
#define EXEC_MODE_SUSPENDED 2
#define EXEC_MODE_EXITED 4

struct thread_capability {
    /// the root capability of this thread, all of its capability addresses are relative to this
    struct capability root_capability;
    /// the execution mode of this thread
    uint8_t exec_mode;
    /// niceness value of this thread (-20 to 20)
    int8_t niceness;
    /// estimate of how much CPU time this thread has used recently in 17.14 fixed point
    uint32_t recent_cpu_time;
    /// the saved registers of this thread
    struct thread_registers registers;
    /// various flags used to define the state of this thread
    uint8_t flags;
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
    /// linked list forming a queue of threads that are blocked on an endpoint
    LIST_LINK(struct thread_capability) blocked_queue;
    /// the endpoint that this thread is blocked on
    struct endpoint_capability *blocked_on;
    /// contains the message struct that was passed to an IPC call
    struct ipc_message *message_buffer;
    /// if this thread is sending a message, this contains the badge of the endpoint that was used to send it
    size_t sending_badge;
};

extern struct invocation_handlers thread_handlers;

void init_threads(void);
bool look_up_thread_by_id(uint16_t thread_id, uint8_t bucket_number, struct thread_capability **thread);

#include "heap.h"

struct thread_capability *alloc_thread(struct heap *heap);
