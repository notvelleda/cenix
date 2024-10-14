#include "debug.h"
#include "namespaces.h"
#include <stddef.h>
#include <stdbool.h>
#include "structures.h"
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/vfs.h"

#define INIT_NODE_DEPTH 4

void _start(void) {
    printf("hellorld from vfs server!\n");

    init_vfs_structures();

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

    set_up_filesystem_for_process(1, 1, 0, 2, endpoint_alloc_args.address, node_alloc_args.address, 0, node_alloc_args.size);

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

        size_t fs_id = received.badge >> 1;
        bool can_modify_namespace = received.badge & 1;

        // TODO: properly handle message
        printf("vfs server: got message %d for fs %d (can modify: %s)!\n", received.buffer[0], fs_id, can_modify_namespace ? "true" : "false");

        switch (received.buffer[0]) {
        case VFS_OPEN:
            printf("open (flags 0x%x, mode 0x%x)\n", received.buffer[1], received.buffer[2]);
            break;
        case VFS_MOUNT:
            {
                printf("mount (flags 0x%x)\n", received.buffer[1]);

                // TODO: return an error if can_modify_namespace is false
                struct ipc_message message = {
                    .capabilities = {}
                };
                *(size_t *) &message.buffer = mount(fs_id, received.capabilities[1].address, received.capabilities[2].address, received.buffer[1]);

                syscall_invoke(received.capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &message);
            }
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

                size_t result = set_up_filesystem_for_process(
                    creator_pid,
                    new_pid,
                    received.buffer[1],
                    received.capabilities[0].address,
                    endpoint_alloc_args.address,
                    node_alloc_args.address,
                    IPC_CAPABILITY_SLOTS + 1,
                    node_alloc_args.size
                );

                if (result != 0) {
                    printf("set_up_filesystem_for_process failed with error %d\n", result);

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
