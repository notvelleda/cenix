#include "debug.h"
#include "directories.h"
#include "ipc.h"
#include "namespaces.h"
#include <stddef.h>
#include <stdbool.h>
#include "structures.h"
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/vfs.h"
#include "sys/stat.h"

void _start(void) {
    printf("hellorld from vfs server!\n");

    // sanity check :3
    if (sizeof(struct stat) >= 64 - sizeof(size_t)) {
        syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "stat structure is too big!\n");
        while (1);
    }

    init_vfs_structures();

    // allocate a new endpoint to handle all client <-> vfs communication
    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    set_up_filesystem_for_process(1, 1, 0, 2, endpoint_alloc_args.address, 0, 0);

    // main vfs server loop

    size_t thread_id = 0;
    struct ipc_message received;

    for (int i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
        received.capabilities[i].address = THREAD_STORAGE_SLOT(thread_id, i);
        received.capabilities[i].depth = THREAD_STORAGE_SLOT_DEPTH;
    }

    while (1) {
        size_t result = syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &received);

        if (result != 0) {
            printf("vfs server: receive call failed with error %d\n", result);
            continue;
        }

        // TODO: hand off received messages to worker threads with maybe some kind of timeout so they can be killed if a process or fs server is unresponsive
        // TODO: permissions checks in open calls

        const size_t temp_slot = IPC_CAPABILITY_SLOTS + 1;

        if (IPC_FLAGS(received.badge) == IPC_FLAG_IS_DIRECTORY) {
            // handle directory proxy calls
            handle_directory_message(thread_id, &received, endpoint_alloc_args.address, temp_slot);
        } else if (IPC_FLAGS(received.badge) == IPC_FLAG_IS_MOUNT_POINT) {
            // handle mount point directory calls
            handle_mount_point_message(thread_id, &received, endpoint_alloc_args.address, temp_slot);
        } else {
            // handle vfs calls

            size_t namespace_id = IPC_ID(received.badge);
            bool can_modify_namespace = IPC_FLAGS(received.badge) == IPC_FLAG_CAN_MODIFY;

            printf("vfs server: got message %d for fs %d (can modify: %s)!\n", FD_CALL_NUMBER(received), namespace_id, can_modify_namespace ? "true" : "false");

            struct ipc_message reply = {
                .capabilities = {}
            };

            switch (FD_CALL_NUMBER(received)) {
            case VFS_OPEN_ROOT:
                printf("open root\n");

                result = open_root(thread_id, &received, endpoint_alloc_args.address, temp_slot, namespace_id);

                if (result != 0) {
                    printf("open_root failed with error %d\n", result);
                    FD_RETURN_VALUE(reply) = ENOSYS;
                    syscall_invoke(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);
                }

                break;
            case VFS_MOUNT:
                printf("mount (flags 0x%x)\n", received.buffer[1]);
                FD_RETURN_VALUE(reply) = can_modify_namespace ? mount(namespace_id, received.capabilities[1].address, received.capabilities[2].address, received.buffer[1]) : EPERM;
                syscall_invoke(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);

                break;
            case VFS_UNMOUNT:
                printf("unmount\n");

                // TODO: this
                FD_RETURN_VALUE(reply) = ENOSYS;
                syscall_invoke(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);

                break;
            case VFS_NEW_PROCESS:
                {
                    // TODO: verify that this is coming from the process server
                    pid_t new_pid = (received.buffer[2] << 8) | received.buffer[3];
                    pid_t creator_pid = (received.buffer[4] << 8) | received.buffer[5];

                    printf("new process (pid %d, creator %d, flags 0x%x)\n", new_pid, creator_pid, received.buffer[1]);

                    result = set_up_filesystem_for_process(
                        creator_pid,
                        new_pid,
                        received.buffer[1],
                        FD_REPLY_ENDPOINT(received).address,
                        endpoint_alloc_args.address,
                        thread_id,
                        temp_slot
                    );

                    if (result != 0) {
                        printf("set_up_filesystem_for_process failed with error %d\n", result);

                        FD_RETURN_VALUE(reply) = result;

                        syscall_invoke(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);
                    }
                }

                break;
            default:
                FD_RETURN_VALUE(reply) = EBADMSG;
                syscall_invoke(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);
            }
        }

        // delete any leftover capabilities that were transferred
        for (size_t i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
            if ((received.transferred_capabilities & (1 << i)) != 0) {
                syscall_invoke(THREAD_STORAGE_ADDRESS(thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, i);
            }
        }
    }
}
