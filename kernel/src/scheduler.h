#pragma once

#include "arch.h"
#include "threads.h"
#include <stdbool.h>

#define NUM_PRIORITIES 64

struct scheduler_state {
    /// the thread that's currently being executed
    struct thread_capability *current_thread;
    /// the priority queues for threads that need to be executed
    struct thread_queue priority_queues[NUM_PRIORITIES];
    /// a queue of threads that need their cpu time values updated
    struct thread_queue needs_cpu_time_update;
    /// how many times per second timer ticks occur
    uint8_t timer_hz;
    /// how many timer ticks remain until the next cpu time update
    uint8_t ticks_until_cpu_time_update;
    /// whether there's a pending context switch
    bool pending_context_switch;
};

extern struct scheduler_state scheduler_state;

void init_scheduler(void);
void resume_thread(struct thread_capability *thread);
void suspend_thread(struct thread_capability *thread);
void yield_thread(void);
void try_context_switch(struct thread_registers *registers);
