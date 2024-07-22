#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "debug.h"

// if this flag is set, the resource managed by this capability is allocated on and managed in the heap,
// and as such references to it should be properly updated by the heap manager
#define CAP_FLAG_IS_HEAP_MANAGED 1

struct capability {
    // the invocation handlers for this capability
    struct invocation_handlers *handlers;
    // the resource that this capability points to
    void *resource;
    // additional flags describing the state of this capability
    uint8_t flags;
    // identifier that can be set to differentiate this specific capability from otherwise identical ones
    size_t badge;
};

#define MAX_HANDLERS 4

struct invocation_handlers {
    size_t num_handlers;
    size_t (*handlers[MAX_HANDLERS])(struct capability *slot, size_t argument, bool from_userland);
};

// invokes the provided handler number on the capability and returns the result
inline size_t capability_invoke(struct capability *slot, size_t handler_number, size_t argument, bool from_userland) {
    struct invocation_handlers *handlers = slot->handlers;

#ifdef DEBUG
    if (handler_number >= handlers->num_handlers) {
        printk("tried to invoke an out of bounds capability handler!\n");
        return 0;
    } else {
#endif
        if (handlers == NULL) {
            printk("attempted to invoke an invalid capability!\n");
            return 0;
        } else {
            return handlers->handlers[handler_number](slot, argument, from_userland);
        }
#ifdef DEBUG
    }
#endif
}

// updates the address of a capability's resource, given its address in capability space
void update_capability_resource(size_t address, size_t depth, void *new_resource_address);

#define TYPE_UNTYPED 0 // is this a good name for user-modifiable memory?
#define TYPE_NODE 1

struct alloc_args {
    // the type of the object to create
    uint8_t type;
    // if this object can have various sizes, this value determines the size of the object
    uint8_t size;
    // the address at which the capability to this object should be placed at
    size_t address;
    // how many bits of the address field are valid and should be used to search
    size_t depth;
};

#include "heap.h"

// initializes the kernel's root capability
void init_root_capability(struct heap *heap);
