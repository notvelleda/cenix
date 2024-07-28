#include "sys/kernel.h"

static void test_thread(void);

#define STACK_SIZE 4096

void _start(void) {
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "hello from init!\n");

    struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = STACK_SIZE,
        .address = 2,
        .depth = 4
    };
    syscall_invoke(0, 4, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    void *stack_base = (void *) syscall_invoke(2, 4, UNTYPED_LOCK, 0);
    size_t stack_pointer = (size_t) stack_base + STACK_SIZE;

    struct alloc_args thread_alloc_args = {
        .type = TYPE_THREAD,
        .size = 0,
        .address = 3,
        .depth = 4
    };
    syscall_invoke(0, 4, ADDRESS_SPACE_ALLOC, (size_t) &thread_alloc_args);

    struct thread_registers registers;

    set_program_counter(&registers, (size_t) &test_thread);
    set_stack_pointer(&registers, stack_pointer);

    // TODO: find a better way of doing this
    uint32_t got_pointer;
    __asm__ __volatile__ ("movel %%a5, %0" : "=r" (got_pointer));
    set_got_pointer(&registers, (size_t) got_pointer);

    struct read_write_register_args register_write_args = {
        .address = &registers,
        .size = sizeof(struct thread_registers)
    };
    syscall_invoke(3, 4, THREAD_WRITE_REGISTERS, (size_t) &register_write_args);
    syscall_invoke(3, 4, THREAD_RESUME, 0);

    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 4,
        .depth = 4,
    };
    syscall_invoke(0, 4, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 4,
        .address = 5,
        .depth = 4,
    };
    syscall_invoke(0, 4, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args);

    struct node_copy_args alloc_copy_args = {
        .source_address = endpoint_alloc_args.address,
        .source_depth = endpoint_alloc_args.depth,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_COPY, (size_t) &alloc_copy_args);

    struct node_copy_args debug_copy_args = {
        .source_address = 1,
        .source_depth = 4,
        .dest_slot = 1,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_COPY, (size_t) &debug_copy_args);

    struct set_root_node_args set_root_node_args = {node_alloc_args.address, node_alloc_args.depth};
    syscall_invoke(3, 4, THREAD_SET_ROOT_NODE, (size_t) &set_root_node_args);

    syscall_invoke(3, 4, THREAD_RESUME, 0);

    struct ipc_message send = {
        .buffer = {'H', 'e', 'l', 'l', 'o', 'r', 'l', 'd', '!', 0},
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "thread 1: sending message \"");
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &send.buffer);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "\"\n");
    syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_SEND, (size_t) &send);

    struct ipc_message receive = {
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &receive);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "thread 1: received message \"");
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &receive.buffer);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "\"\n");

    while (1) {
        syscall_yield();
    }
}

static void test_thread(void) {
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "thread 2 started!\n");

    struct ipc_message receive = {
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(0, 4, ENDPOINT_RECEIVE, (size_t) &receive);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "thread 2: received message \"");
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &receive.buffer);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "\"\n");

    struct ipc_message send = {
        .buffer = {'U', 'w', 'U', 0},
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "thread 2: sending message \"");
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &send.buffer);
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "\"\n");
    syscall_invoke(0, 4, ENDPOINT_SEND, (size_t) &send);

    while (1) {
        syscall_yield();
    }
}

#include "../../kernel/src/memset.c"
#include "../../kernel/src/memcpy.c"
