#pragma once

#include <stdint.h>
#include <stddef.h>

struct arguments_data;

#if defined(__m68k__) || defined(M68000) || defined(__MC68K__)
#include "./arch/68000.h"
#endif

// if this program is being tested, the syscall functions will be defined in the test harness instead of in a file included here
#ifdef UNDER_TEST
#define __SYSCALL_PREFIX
#else
#define __SYSCALL_PREFIX static inline
#endif

/// yields the time slice of the current thread, allowing other threads the opportunity to execute instead
__SYSCALL_PREFIX void syscall_yield(void);

/// \brief invokes the function corresponding to the provided handler number on a capability and returns the result
///
/// if the capability is not able to be located or if the given handler number is invalid, 0 will be returned
/// and no operation will be performed. otherwise, the return value depends on the specific invocation made
__SYSCALL_PREFIX size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument);

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
    /// \brief if this is set to a non-zero value, the new capability will have its badge set. if not, its badge will remain unchanged.
    ///
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

/// the handler number for the `untyped_sizeof` invocation
#define UNTYPED_SIZEOF 3

/// the handler number for the `address_space_alloc` invocation
#define ADDRESS_SPACE_ALLOC 0

#define TYPE_UNTYPED 0 // is this a good name for user-modifiable memory?
#define TYPE_NODE 1
#define TYPE_THREAD 2
#define TYPE_ENDPOINT 3

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
    struct thread_registers *address;
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

/// the handler number for the `endpoint_send` invocation
#define ENDPOINT_SEND 0

/// the handler number for the `endpoint_receive` invocation
#define ENDPOINT_RECEIVE 1

// TODO: should there be an endpoint_call invocation that combines send and receive, but always copies the capability in slot 0
// (and fails if one isn't provided) with send only access, then waits for a response on it?

/// the handler number for the `debug_print` invocation
#define DEBUG_PRINT 0

/// the size of the IPC message buffer
#define IPC_BUFFER_SIZE 64

/// how many capabilities can be transferred in a single IPC call
#define IPC_CAPABILITY_SLOTS 4

/// the depth of the process server's (and some other things' too :3) root capability node
#define INIT_NODE_DEPTH 4

/// arguments passed to the `endpoint_send` and `endpoint_receive` invocations
struct ipc_message {
    /// raw data to be copied to the receiving thread or to be copied from the sending thread, depending on the invocation
    uint8_t buffer[IPC_BUFFER_SIZE];
    // TODO: should there be a field that stores how many bytes of this buffer have been written to, so that no more data than needs to be copied has to be?
    /// \brief a list of addresses of capability slots
    ///
    /// when sending a message, any of these fields with a depth of greater than 0 will be moved
    /// to the corresponding slot address in the structure provided to a receive call
    struct ipc_capability {
        /// the address of the capability or available slot
        size_t address;
        /// how many bits of the address field are valid and should be used to search
        /// through the calling thread's address space
        // TODO: should this be a uint8_t? i don't see why this should be able to represent any values over sizeof(size_t) * 8
        size_t depth;
    } capabilities[IPC_CAPABILITY_SLOTS];
    /// \brief if a capability's corresponding bit here is set, it will be copied to the receiving thread instead of moved
    ///
    /// this field is ignored on `endpoint_receive` invocations
    uint8_t to_copy;
    /// when a capability is successfully transferred from the sending thread to the receiving thread, its corresponding bit will be set here
    uint8_t transferred_capabilities;
    /// \brief the badge of the capability that sent this message.
    ///
    /// this field is ignored on `endpoint_send` invocations
    // TODO: should this really be a size_t? maybe it should be hardcoded as like a uint16_t or uint32_t instead
    size_t badge;
};

/// sets the program counter in a register context object to the specified value
void set_program_counter(struct thread_registers *registers, size_t program_counter);

/// sets the stack pointer in a register context object to the specified value
void set_stack_pointer(struct thread_registers *registers, size_t stack_pointer);

/// \brief sets the global offset table (GOT) pointer in a register context to the specified value
///
/// it's entirely architecture-dependent whether this function is implemented or not,
/// and it may not be usable after the thread whose registers it'll update has started
void set_got_pointer(struct thread_registers *registers, size_t got_pointer);

/// \brief starts pushing arguments onto the stack (or the equivalent) for the current architecture's C calling convention
///
/// the stack pointer value of the given registers struct must be valid and pointing to a valid stack (i.e. not overlapping any areas of memory that shouldn't be written to in that manner),
/// otherwise behavior is invalid.
///
/// the arguments `arguments_count` and `arguments_size_bytes` refer to the number of arguments that will be passed and the total size of those arguments in bytes, respectively
void start_arguments(struct arguments_data *data, struct thread_registers *registers, size_t arguments_count, size_t arguments_size_bytes);

/// \brief pushes arguments onto the stack (or the equivalent) for the current architecture's C calling convention
///
/// must be called after a call to `start_arguments` and must be called in the order the arguments appear in the function declaration left to right.
///
/// the same safety requirements for the stack pointer as with `start_arguments` apply here
void add_argument(struct arguments_data *data, struct thread_registers *registers, size_t argument, uint8_t argument_size_bytes);
