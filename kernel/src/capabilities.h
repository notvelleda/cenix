#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "debug.h"
#include "sys/kernel.h"

/// if this flag is set, the resource managed by this capability is allocated on and managed in the heap,
/// and as such references to it should be properly updated by the heap manager
#define CAP_FLAG_IS_HEAP_MANAGED 1

/// if this flag is set, this capability is the root capability of a thread
#define CAP_FLAG_IS_ROOT 2

struct capability {
    /// the invocation handlers for this capability
    struct invocation_handlers *handlers;
    /// the resource that this capability points to
    void *resource;
    /// additional flags describing the state of this capability
    uint8_t flags;
    /// identifier that can be set to differentiate this specific capability from otherwise identical ones
    size_t badge;
    /// flags that describe the ways in which a thread can access this capability's resources
    access_flags_t access_rights;
    /// used to keep track of all the capabilities that depend on this resource,
    /// so that all of their pointers to the resource can be updated when it's moved
    struct {
        struct capability *start;
        struct capability *end;
        struct capability *next;
        struct capability *prev;
    } resource_list;
    /// used to keep track of all the capabilities that share a derivation source with this one,
    /// so that they can be revoked later on if required
    struct {
        struct capability *next;
        struct capability *prev;
    } derivation_list;
    /// points to the start and end of the list of capabilities that have been derived from this one, respectively
    struct capability *derivation_list_start;
    struct capability *derivation_list_end;
    /// points to the capability that this capability was derived from
    struct capability *derived_from;
};

struct absolute_capability_address {
    /// the id of the thread that this capability belongs to
    uint16_t thread_id;
    /// the id of the bucket that the thread is in
    uint8_t bucket_number;
    /// the address of the capability in the thread's capability space
    size_t address;
    /// how far to search into the thread's capability space
    size_t depth;
};

extern struct capability kernel_root_capability;

/// moves a capability between two slots, removing it from the slot it came from
void move_capability(struct capability *from, struct capability *to);

/// updates the address of a capability's resource, given its address in capability space
void update_capability_resource(struct absolute_capability_address *address, void *new_resource_address);

struct look_up_result {
    struct capability *slot;
    void *container;
    bool should_unlock;
};

/// \brief looks up a capability from its address within the given capability node
///
/// on success, true is returned. on failure, false is returned
bool look_up_capability(struct capability *root, size_t address, size_t depth, struct look_up_result *result);

/// \brief looks up a capability relative to either the kernel root capability or the current thread's root capability
///
/// on success, true is returned. on failure, false is returned
bool look_up_capability_relative(size_t address, size_t depth, bool from_userland, struct look_up_result *result);

/// \brief looks up a capability relative to a given thread's root capability node
///
/// on success, true is returned. on failure, false is returned
bool look_up_capability_absolute(struct absolute_capability_address *address, struct look_up_result *result);

// TODO: find a better name for this
void unlock_looked_up_capability(struct look_up_result *result);

/// the maximum number of invocation handlers that a capability can have
#define MAX_HANDLERS 5

struct invocation_handlers {
    /// how many invocation handlers in this struct are valid
    size_t num_handlers;
    /// the list of invocation handlers, stored as function pointers to handler functions
    size_t (*handlers[MAX_HANDLERS])(struct capability *slot, size_t argument, bool from_userland);
};

/// invocation handlers for capability nodes
extern struct invocation_handlers node_handlers;

/// invocation handlers for untyped capabilities
extern struct invocation_handlers untyped_handlers;

/// invocation handlers for address space capabilities
extern struct invocation_handlers address_space_handlers;

/// in-kernel equivalent of syscall_invoke()
size_t invoke_capability(size_t address, size_t depth, size_t handler_number, size_t argument, bool from_userland);

#include "heap.h"

/// initializes the kernel's root capability
void init_root_capability(struct heap *heap);

/// the value of size_bits for the kernel's root capability node
#define ROOT_CAP_SLOT_BITS 4
