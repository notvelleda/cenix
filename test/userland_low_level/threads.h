#pragma once

#include "capabilities.h"
#include <stdint.h>

struct thread_capability {
    struct capability root_capability;
    uint16_t thread_id;
    uint8_t bucket_number;
};

extern struct invocation_handlers thread_handlers;
static inline struct thread_capability *alloc_thread(struct heap *heap) {
    return NULL;
}

#include "scheduler.h"

static inline bool look_up_thread_by_id(uint16_t thread_id, uint8_t bucket_number, struct thread_capability **thread) {
    *thread = scheduler_state.current_thread;
    return heap_lock(scheduler_state.current_thread);
}
