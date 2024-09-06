#pragma once

#include "arch.h"
#include "threads.h"
#include "linked_list.h"
#include <stdbool.h>

#define NUM_PRIORITIES 64

/// the state of the scheduler
struct scheduler_state {
    /// the thread that's currently being executed
    struct thread_capability *current_thread;
    /// the priority queues for threads that need to be executed
    LIST_CONTAINER(struct thread_capability) priority_queues[NUM_PRIORITIES];
    /// a queue of threads that need their cpu time values updated
    LIST_CONTAINER(struct thread_capability) needs_cpu_time_update;
    /// how many times per second timer ticks occur
    uint8_t timer_hz;
    /// how many timer ticks remain until the next cpu time update
    uint8_t ticks_until_cpu_time_update;
    /// whether there's a pending context switch
    bool pending_context_switch;
};

extern struct scheduler_state scheduler_state;

/// initializes the scheduler
void init_scheduler(void);

/// \brief resumes a thread, allowing it to resume execution, if the given execution mode matches its current execution mode
///
/// if the execution modes don't match, nothing will happen
void resume_thread(struct thread_capability *thread, uint8_t reason);

/// \brief suspends a thread, setting its execution mode to the one provided
///
/// if the thread is already suspended, nothing will happen
void suspend_thread(struct thread_capability *thread, uint8_t new_exec_mode);

/// yields the rest of the time slice for the current thread, allowing other threads to execute
void yield_thread(void);

void handle_thread_exception(struct thread_registers *registers, const char *cause);

/// if a context switch has been requested (i.e. by suspending a thread), this function will perform it
void try_context_switch(struct thread_registers *registers);
