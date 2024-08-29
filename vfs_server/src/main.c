#include "debug.h"
#include "sys/kernel.h"
#include <stddef.h>

void _start(void) {
    printf("hello from vfs server!\n");

    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    const struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 2,
        .address = 4,
        .depth = 4
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args);

    const struct node_copy_args copy_args = {
        .source_address = endpoint_alloc_args.address,
        .source_depth = endpoint_alloc_args.depth,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 1
    };
    syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_COPY, (size_t) &copy_args);

    struct ipc_message message = {
        .capabilities = {
            {(copy_args.dest_slot << node_alloc_args.depth) | node_alloc_args.address, node_alloc_args.depth + node_alloc_args.size},
            {0, 0},
            {0, 0},
            {0, 0}
        }
    };
    syscall_invoke(2, -1, ENDPOINT_SEND, (size_t) &message);

    printf("sent message! waiting for response\n");

    struct ipc_message message2 = {
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &message2);

    printf("got response!\n");

    while (1) {
        syscall_yield();
    }
}
