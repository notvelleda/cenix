#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined(__m68k__) || defined(M68000) || defined(__MC68K__)
#include "./arch/68000.h"
#endif

/// yields the time slice of the current thread, allowing other threads the opportunity to execute instead
static inline void syscall_yield(void);

/// \brief invokes the function corresponding to the provided handler number on a capability and returns the result
///
/// if the capability is not able to be located or if the given handler number is invalid, 0 will be returned
/// and no operation will be performed. otherwise, the return value depends on the specific invocation made
static inline size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument);

/// flags that describe the access rights for a given capability
typedef uint8_t access_flags_t;

/// the handler number for the `node_copy` invocation
#define NODE_COPY 0

/// arguments passed to the `node_copy` invocation on a capability node
struct node_copy_args {
    /// the address of the capability to copy
    size_t source_address;
    /// how many bits of the source_address field are valid and should be used to search
    /// through the calling thread's address space
    size_t source_depth;
    /// the index of the slot in this node to copy the capability into
    size_t dest_slot;
    /// the access rights of the new capability.
    /// if any bits set here aren't set in the source capability's access rights field, they won't be set in the copied capability
    access_flags_t access_rights;
    /// the badge of the new capability
    size_t badge;
    /// if this is set to a non-zero value, the new capability will have its badge set. if not, its badge will remain unchanged.
    /// badges can only be set on original capabilities (capabilities that have not been derived from other capabilities).
    /// if an attempt is made to set the badge on a non-original capability, the copy invocation will fail
    uint8_t should_set_badge;
};

/// the handler number for the `node_move` invocation
#define NODE_MOVE 1

/// arguments passed to the `node_move` invocation on a capability node
struct node_move_args {
    /// the address of the capability to move
    size_t source_address;
    /// how many bits of the source_address field are valid and should be used to search
    /// through the calling thread's address space
    size_t source_depth;
    /// the index of the slot in this node to move the capability into
    size_t dest_slot;
};

/// the handler number for the `node_delete` invocation
#define NODE_DELETE 2

/// the handler number for the `node_revoke` invocation
#define NODE_REVOKE 3

/// the handler number for the `untyped_lock` invocation
#define UNTYPED_LOCK 0

/// the handler number for the `untyped_unlock` invocation
#define UNTYPED_UNLOCK 1

/// the handler number for the `untyped_try_lock` invocation
#define UNTYPED_TRY_LOCK 2

/// the handler number for the `address_space_alloc` invocation
#define ADDRESS_SPACE_ALLOC 0

#define TYPE_UNTYPED 0 // is this a good name for user-modifiable memory?
#define TYPE_NODE 1
#define TYPE_THREAD 2

/// arguments passed to the `address_space_alloc` invocation on an address space capability
struct alloc_args {
    /// the type of the object to create
    uint8_t type;
    /// if this object can have various sizes, this value determines the size of the object
    size_t size;
    /// the address at which the capability to this object should be placed at
    size_t address;
    /// how many bits of the address field are valid and should be used to search
    /// through the calling thread's address space
    size_t depth;
};

/// the handler number for the `thread_read_registers` invocation
#define THREAD_READ_REGISTERS 0

/// the handler number for the `thread_write_registers` invocation
#define THREAD_WRITE_REGISTERS 1

/// the handler number for the `thread_resume` invocation
#define THREAD_RESUME 2

/// the handler number for the `thread_suspend` invocation
#define THREAD_SUSPEND 3

/// the handler number for the `thread_set_root_node` invocation
#define THREAD_SET_ROOT_NODE 4

/// arguments pased to the `thread_read_registers` and `thread_write_registers` invocations on a thread capability
struct read_write_register_args {
    /// the address to read registers from or write registers to
    void *address;
    /// how many bytes of the registers object should be copied
    size_t size;
};

/// arguments passed to the `thread_set_root_node` invocation on a thread capability
struct set_root_node_args {
    /// the address of the capability to use as the root node
    size_t address;
    /// how many bits of the address field are valid and should be used to search
    /// through the calling thread's address space
    size_t depth;
};
