// scheduler based on the 4.4BSD scheduler as described in https://www.scs.stanford.edu/23wi-cs212/pintos/pintos_7.html

#include "scheduler.h"
#include "arch.h"
#include "debug.h"
#include "heap.h"

struct scheduler_state scheduler_state;

void init_scheduler(void) {
    scheduler_state.current_thread = NULL;
    for (int i = 0; i < NUM_PRIORITIES; i ++) {
        scheduler_state.priority_queues[i].start = NULL;
        scheduler_state.priority_queues[i].end = NULL;
    }
    scheduler_state.needs_cpu_time_update.start = NULL;
    scheduler_state.needs_cpu_time_update.end = NULL;
    scheduler_state.timer_hz = 0;
    scheduler_state.ticks_until_cpu_time_update = 0;
}

void thread_queue_add(struct thread_queue *queue, struct thread_capability *thread) {
    if (queue->start == NULL) {
        queue->start = thread;
        queue->end = thread;
    } else {
        bool should_unlock = heap_lock(queue->end);

        queue->end->runqueue_entry.next = thread;
        thread->runqueue_entry.prev = queue->end;
        queue->end = thread;

        if (should_unlock) {
            heap_unlock(queue->end);
        }
    }
}

void queue_thread(struct thread_capability *thread) {
    if (thread->runqueue != NULL) {
        return;
    }

    // TODO: calculate priority
    thread_queue_add(&scheduler_state.priority_queues[63], thread);
}

void resume_thread(struct thread_capability *thread) {
    queue_thread(thread);

    if (scheduler_state.current_thread == NULL) {
        scheduler_state.pending_context_switch = true;
    }

    thread->exec_mode = RUNNING; // TODO: check if it's blocked on anything
}

void suspend_thread(struct thread_capability *thread) {
    if (scheduler_state.current_thread == thread) {
        scheduler_state.pending_context_switch = true;
    }

    // TODO
}

void yield_thread(void) {
    printk("TODO: yield thread\n");
}

// code that runs when no other process is running
// TODO: use the arch abstraction layer to put the cpu into some kind of low power/sleep mode or something until the next interrupt
static void idle_thread(void) {
    while (1);
}

void try_context_switch(struct registers *registers) {
    if (!scheduler_state.pending_context_switch) {
        return;
    }

    // find the next thread that should be executed
    struct thread_capability *next_thread = NULL;
    bool should_unlock_next = false;

    for (int i = NUM_PRIORITIES - 1; i >= 0; i --) {
        struct thread_queue *queue = &scheduler_state.priority_queues[i];

        while (queue->start != NULL) {
            // pop the thread off of the queue
            should_unlock_next = heap_lock(queue->start);
            next_thread = queue->start;
            queue->start = next_thread->runqueue_entry.next;

            next_thread->runqueue = NULL;
            next_thread->runqueue_entry.prev = NULL;
            next_thread->runqueue_entry.next = NULL;

            if (next_thread->exec_mode != RUNNING) {
                // if this thread shouldn't be executed, keep searching until one that should is found
                if (should_unlock_next) {
                    heap_unlock(next_thread);
                    should_unlock_next = false;
                }

                next_thread = NULL;
                continue;
            }

            goto found_thread;
        }
    }

found_thread:

    bool should_idle = true; // whether the idle thread should be entered

    // save state of current thread and queue it up for execution if it needs more cpu time
    if (scheduler_state.current_thread != NULL) {
        bool should_unlock = heap_lock(scheduler_state.current_thread);
        struct thread_capability *thread = scheduler_state.current_thread;

        if (thread->exec_mode == RUNNING) {
            // only save this thread's registers and queue it if it's being preempted by another thread
            if (next_thread != NULL) {
                next_thread->registers = *registers;
                next_thread->scheduler_flags &= ~THREAD_CURRENTLY_RUNNING;
                queue_thread(thread);
            }

            should_idle = false;
        } else {
            // save this thread's registers since it's pausing execution
            next_thread->registers = *registers;
            next_thread->scheduler_flags &= ~THREAD_CURRENTLY_RUNNING;
            scheduler_state.current_thread = NULL;
        }

        if (should_unlock) {
            heap_unlock(thread);
        }
    }

    // load state of next thread
    if (next_thread != NULL) {
        *registers = next_thread->registers;
        scheduler_state.current_thread = next_thread;
        next_thread->scheduler_flags |= THREAD_CURRENTLY_RUNNING;

        if (should_unlock_next) {
            heap_unlock(next_thread);
        }
    } else if (should_idle) {
        // enter the idle thread since there's nothing to do
        set_program_counter(registers, (size_t) &idle_thread);
    }
}
