#pragma once

#include "capabilities.h"
#include "threads.h"
#include "heap.h"

struct endpoint_capability {
    /// a queue of threads that are blocked trying to send messages to this endpoint
    LIST_CONTAINER(struct thread_capability) blocked_sending;
    /// a queue of threads that are blocked trying to receive from this endpoint
    LIST_CONTAINER(struct thread_capability) blocked_receiving;
};

/// invocation handlers for endpoints
extern struct invocation_handlers endpoint_handlers;

/// allocates a new endpoint on the given heap and returns a pointer to it
struct endpoint_capability *alloc_endpoint(struct heap *heap);
