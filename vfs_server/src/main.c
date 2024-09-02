#include "debug.h"
#include <stddef.h>
#include "sys/misc.h"
#include "sys/kernel.h"
#include "sys/vfs.h"

#define INIT_NODE_DEPTH 4

void _start(void) {
    printf("hellorld from vfs server!\n");

    // allocate a new endpoint to handle all client <-> vfs communication
    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    // since the root capability of this thread's capability space can't be modified, a new capability node needs to be created
    const struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 3,
        .address = 4,
        .depth = INIT_NODE_DEPTH
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args);

    // TODO: set up vfs data structures for the process server

    // set up badged endpoint for process 1 (process server)
    const struct node_copy_args copy_args = {
        .source_address = endpoint_alloc_args.address,
        .source_depth = endpoint_alloc_args.depth,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 1,
        .should_set_badge = 1
    };
    syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_COPY, (size_t) &copy_args);

    // the vfs server is started with an endpoint in its capability space that can be used to communicate with the process server,
    // so the actual endpoint that they'll use for communication needs to be sent back to the process server using it
    struct ipc_message message = {
        .capabilities = {{(copy_args.dest_slot << node_alloc_args.depth) | node_alloc_args.address, node_alloc_args.depth + node_alloc_args.size}}
    };
    syscall_invoke(2, -1, ENDPOINT_SEND, (size_t) &message);

    // main vfs server loop

    struct ipc_message received;

    for (int i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
        received.capabilities[i].address = (i << node_alloc_args.depth) | node_alloc_args.address;
        received.capabilities[i].depth = node_alloc_args.depth + node_alloc_args.size;
    }

    while (1) {
        size_t result = syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &received);

        if (result != 0) {
            printf("vfs server: receive call failed with error %d\n", result);
            continue;
        }

        // TODO: properly handle message
        printf("vfs server: got message %d from process %d!\n", received.buffer[0], received.badge);

        switch (received.buffer[0]) {
        case VFS_OPEN:
            printf("open (flags 0x%x, mode 0x%x)\n", received.buffer[1], received.buffer[2]);
            break;
        case VFS_MOUNT:
            printf("mount (flags 0x%x)\n", received.buffer[1]);
            break;
        case VFS_BIND:
            printf("bind (flags 0x%x)\n", received.buffer[1]);
            break;
        case VFS_UNMOUNT:
            printf("unmount\n");
            break;
        case VFS_NEW_PROCESS:
            {
                // TODO: verify that this is coming from the process server
                pid_t new_pid = (received.buffer[2] << 8) | received.buffer[3];
                pid_t creator_pid = (received.buffer[4] << 8) | received.buffer[5];

                printf("new process (pid %d, creator %d, flags 0x%x)\n", new_pid, creator_pid, received.buffer[1]);

                // make a new endpoint with the badge being the pid of the new process
                const struct node_copy_args copy_args = {
                    .source_address = endpoint_alloc_args.address,
                    .source_depth = endpoint_alloc_args.depth,
                    .dest_slot = IPC_CAPABILITY_SLOTS + 1,
                    .access_rights = -1,
                    .badge = new_pid,
                    .should_set_badge = 1
                };
                size_t result = syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_COPY, (size_t) &copy_args);

                if (result == 0) {
                    // send it back to the process server
                    struct ipc_message message = {
                        .capabilities = {{(copy_args.dest_slot << node_alloc_args.depth) | node_alloc_args.address, node_alloc_args.depth + node_alloc_args.size}}
                    };
                    syscall_invoke(received.capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &message);
                } else {
                    printf("node copy failed with error %d\n", result);

                    struct ipc_message message = {
                        .capabilities = {}
                    };
                    *(size_t *) &message.buffer = result;

                    syscall_invoke(received.capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &message);
                }
            }

            break;
        }

        // delete any leftover capabilities that were transferred
        for (size_t i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
            if ((received.transferred_capabilities & (1 << i)) != 0) {
                syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_DELETE, i);
            }
        }
    }

    while (1) {
        syscall_yield();
    }
}
