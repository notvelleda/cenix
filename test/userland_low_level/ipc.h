#pragma once

#include "capabilities.h"
#include "heap.h"

struct endpoint_capability {};

static inline struct endpoint_capability *alloc_endpoint(struct heap *heap) {
    return (struct endpoint_capability *) heap_alloc(heap, sizeof(struct endpoint_capability));
}

extern struct invocation_handlers endpoint_handlers;
