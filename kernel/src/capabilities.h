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

extern struct capability kernel_root_capability;

// updates the address of a capability's resource, given its address in capability space
void update_capability_resource(size_t address, size_t depth, void *new_resource_address);

struct look_up_result {
    struct capability *slot;
    struct capability_node_header *container;
    bool should_unlock;
};

bool look_up_capability(struct capability *root, size_t address, size_t depth, struct look_up_result *result);

// TODO: find a better name for this
void unlock_looked_up_capability(struct look_up_result *result);

#define MAX_HANDLERS 4

struct invocation_handlers {
    size_t num_handlers;
    size_t (*handlers[MAX_HANDLERS])(struct capability *slot, size_t argument, bool from_userland);
};

// invokes the provided handler number on a capability and returns the result.
// if the capability is not able to be located for invoking or if the given handler number is invalid, 0 will be returned
// and no operation will be performed
size_t invoke_capability(size_t address, size_t depth, size_t handler_number, size_t argument, bool from_userland);

#define UNTYPED_LOCK 0
#define UNTYPED_UNLOCK 1
#define UNTYPED_TRY_LOCK 2
#define ADDRESS_SPACE_ALLOC 0

#define TYPE_UNTYPED 0 // is this a good name for user-modifiable memory?
#define TYPE_NODE 1
#define TYPE_THREAD 2

struct alloc_args {
    // the type of the object to create
    uint8_t type;
    // if this object can have various sizes, this value determines the size of the object
    size_t size;
    // the address at which the capability to this object should be placed at
    size_t address;
    // how many bits of the address field are valid and should be used to search
    size_t depth;
};

#include "heap.h"

// initializes the kernel's root capability
void init_root_capability(struct heap *heap);

// the value of size_bits for the kernel's root capability node
#define ROOT_CAP_SLOT_BITS 4
