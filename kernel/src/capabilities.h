#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "debug.h"
#include "sys/kernel.h"
#include "linked_list.h"

/// if this flag is set, the resource managed by this capability is allocated on and managed in the heap,
/// and as such references to it should be properly updated by the heap manager
#define CAP_FLAG_IS_HEAP_MANAGED 1

/// if this flag is set, this capability is the root capability of a thread
#define CAP_FLAG_IS_ROOT 2

/// if this flag is set, this capability has had its badge set and was derived from a non-badged capability
#define CAP_FLAG_BADGED 4

/// if this flag is set, this capability is the original in a derivation tree
#define CAP_FLAG_ORIGINAL 8

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
    LIST_NO_CONTAINER(struct capability) resource_list;
    /// used to keep track of all the capabilities that share a derivation source with this one,
    /// so that they can be revoked later on if required
    LIST_NO_CONTAINER(struct capability) derivation_list;
    /// points to the capability that this capability was derived from
    struct capability *derived_from;
    /// points to the start of this capability's derivation list, if applicable
    struct capability *derivation;
    /// the absolute address of this capability
    struct absolute_capability_address address;
    /// if this capability is managed by a heap, this points to the heap that manages it
    struct heap *heap;
};

extern struct capability kernel_root_capability;

/// moves a capability between two slots, removing it from the slot it came from
void move_capability(struct capability *from, struct capability *to);

/// deletes a capability, freeing up its resources if there are no more capabilities sharing it
void delete_capability(struct capability *to_delete);

/// updates the address of a capability's resource, given its address in capability space
void update_capability_resource(const struct absolute_capability_address *address, void *new_resource_address);

/// recursively updates addresses and thread ids starting at a given capability
void update_capability_addresses(struct capability *slot, size_t address, size_t depth, uint16_t thread_id, uint8_t bucket_number, uint8_t nesting);

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
bool look_up_capability_absolute(const struct absolute_capability_address *address, struct look_up_result *result);

// TODO: find a better name for this
void unlock_looked_up_capability(struct look_up_result *result);

/// the maximum number of invocation handlers that a capability can have
#define MAX_HANDLERS 5

struct invocation_handlers {
    /// how many invocation handlers in this struct are valid
    size_t num_handlers;
    /// the list of invocation handlers, stored as function pointers to handler functions
    size_t (*handlers[MAX_HANDLERS])(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland);
    /// \brief called when the resource associated with a capability is about to be freed
    ///
    /// this allows for resources to be cleaned up if required and for references to this capability's resource to be removed.
    void (*destructor)(struct capability *slot);
};

/// \brief how many capability nodes can be nested in a given thread's capability space
///
/// this limitation exists to help prevent stack overflows when moving a node into a new thread
/// or when deleting a node
#define MAX_NESTED_NODES 4

/// the header for a capability node
struct capability_node_header {
    /// \brief the size of this node, stored as the amount of bits that size takes up
    ///
    /// this value can be converted to the number of slots in this capability node by shifting 1 left by it (`1 << slot_bits`)
    size_t slot_bits;
    uint8_t nested_nodes;
};

/// gets how deep the tree of nodes is starting from the given capability node
uint8_t get_nested_nodes_depth(const struct capability *node);

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

#ifdef DEBUG
void print_capability_lists(struct capability *capability);
#endif
