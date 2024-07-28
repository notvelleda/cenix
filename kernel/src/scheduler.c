// scheduler based on the 4.4BSD scheduler as described in https://www.scs.stanford.edu/23wi-cs212/pintos/pintos_7.html

#include "scheduler.h"
#include "arch.h"
#include "debug.h"
#include "heap.h"
#include "linked_list.h"

#undef DEBUG_SCHEDULER

struct scheduler_state scheduler_state;

void init_scheduler(void) {
    scheduler_state.current_thread = NULL;
    for (int i = 0; i < NUM_PRIORITIES; i ++) {
        LIST_INIT(scheduler_state.priority_queues[i]);
    }
    LIST_INIT(scheduler_state.needs_cpu_time_update);
    scheduler_state.timer_hz = 0;
    scheduler_state.ticks_until_cpu_time_update = 0;
}

void queue_thread(struct thread_capability *thread) {
    if (thread->runqueue != NULL) {
        return;
    }

    // TODO: calculate priority
    thread->runqueue = &scheduler_state.priority_queues[63];
    LIST_APPEND(*thread->runqueue, runqueue_entry, thread);
}

void resume_thread(struct thread_capability *thread, uint8_t reason) {
    thread->exec_mode &= ~reason;

    if (thread->exec_mode == EXEC_MODE_RUNNING) {
        queue_thread(thread);

        if (scheduler_state.current_thread == NULL) {
            scheduler_state.pending_context_switch = true;
        }
    }
}

void suspend_thread(struct thread_capability *thread, uint8_t new_exec_mode) {
    if (scheduler_state.current_thread == thread) {
        scheduler_state.pending_context_switch = true;
    }

    thread->exec_mode |= new_exec_mode;

    if (thread->exec_mode != EXEC_MODE_RUNNING && thread->runqueue != NULL) {
        LIST_REMOVE(*thread->runqueue, runqueue_entry, thread);
        thread->runqueue = NULL;
    }
}

void yield_thread(void) {
    scheduler_state.pending_context_switch = true;
}

// code that runs when no other process is running
// TODO: use the arch abstraction layer to put the cpu into some kind of low power/sleep mode or something until the next interrupt
static void idle_thread(void) {
    while (1);
}

void try_context_switch(struct thread_registers *registers) {
    if (!scheduler_state.pending_context_switch) {
        return;
    }

    scheduler_state.pending_context_switch = false;

#ifdef DEBUG_SCHEDULER
    printk("scheduler: context switching...\n");
#endif

    // find the next thread that should be executed
    struct thread_capability *next_thread = NULL;

    for (int i = NUM_PRIORITIES - 1; i >= 0; i --) {
        LIST_CONTAINER(struct thread_capability) *queue = &scheduler_state.priority_queues[i];

        if (queue->start != NULL) {
            // pop the thread off of the queue
            LIST_POP_FROM_START(*queue, runqueue_entry, next_thread);
            next_thread->runqueue = NULL;
            goto found_thread;
        }
    }

found_thread:
#ifdef DEBUG_SCHEDULER
    printk("scheduler: next_thread is 0x%x\n", next_thread);
#endif

    bool should_idle = true; // whether the idle thread should be entered

    // save state of current thread and queue it up for execution if it needs more cpu time
    if (scheduler_state.current_thread != NULL) {
        struct thread_capability *thread = scheduler_state.current_thread;

        if (thread->exec_mode == EXEC_MODE_RUNNING) {
            // only save this thread's registers and queue it if it's being preempted by another thread
            if (next_thread != NULL) {
#ifdef DEBUG_SCHEDULER
                printk("scheduler: saving registers for 0x%x due to preemption\n", thread);
#endif
                thread->registers = *registers;
                thread->flags &= ~THREAD_CURRENTLY_RUNNING;
                queue_thread(thread);
            }

            should_idle = false;
        } else {
            // save this thread's registers since it's pausing execution
#ifdef DEBUG_SCHEDULER
            printk("scheduler: saving registers for 0x%x due to pause\n", thread);
#endif
            thread->registers = *registers;
            thread->flags &= ~THREAD_CURRENTLY_RUNNING;
            scheduler_state.current_thread = NULL;
        }
    }

    // load state of next thread
    if (next_thread != NULL) {
#ifdef DEBUG_SCHEDULER
        printk("scheduler: loading registers for 0x%x\n", next_thread);
#endif
        *registers = next_thread->registers;
        scheduler_state.current_thread = next_thread;
        next_thread->flags |= THREAD_CURRENTLY_RUNNING;
    } else if (should_idle) {
#ifdef DEBUG_SCHEDULER
        printk("scheduler: entering idle thread\n");
#endif
        // enter the idle thread since there's nothing to do
        set_program_counter(registers, (size_t) &idle_thread);
    }
}
