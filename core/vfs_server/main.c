#include "debug.h"
#include "directories.h"
#include "ipc.h"
#include "mount_points.h"
#include "namespaces.h"
#include <stddef.h>
#include <stdbool.h>
#include "structures.h"
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/vfs.h"
#include "sys/stat.h"
#include "test_macros.h"

/// handles receiving a message not specific to a given file descriptor (i.e. VFS_* calls) and replying to it
static void handle_vfs_message(const struct state *state, struct ipc_message *message) {
    size_t namespace_id = IPC_ID(message->badge);
    bool can_modify_namespace = IPC_FLAGS(message->badge) == IPC_FLAG_CAN_MODIFY;

    printf("vfs_server: got message %d for fs %d (can modify: %s)\n", FD_CALL_NUMBER(*message), namespace_id, can_modify_namespace ? "true" : "false");

    struct ipc_message reply = {
        .capabilities = {}
    };
    size_t result;

    switch (FD_CALL_NUMBER(*message)) {
    case VFS_OPEN_ROOT:
        printf("vfs_server: VFS_OPEN_ROOT called\n");

        result = open_root(state, message, namespace_id);

        if (result != 0) {
            printf("vfs_server: open_root failed with error %d\n", result);
            FD_RETURN_VALUE(reply) = ENOSYS;
            syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);
        }

        break;
    case VFS_MOUNT:
        printf("vfs_server: VFS_MOUNT called (flags 0x%x)\n", message->buffer[1]);
        FD_RETURN_VALUE(reply) = can_modify_namespace ? mount(namespace_id, message->capabilities[1].address, message->capabilities[2].address, message->buffer[1]) : EPERM;
        syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);

        break;
    case VFS_UNMOUNT:
        printf("vfs_server: VFS_UNMOUNT called\n");

        // TODO: this
        FD_RETURN_VALUE(reply) = ENOSYS;
        syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);

        break;
    case VFS_NEW_PROCESS:
        {
            // TODO: verify that this is coming from the process server
            pid_t new_pid = (message->buffer[2] << 8) | message->buffer[3];
            pid_t creator_pid = (message->buffer[4] << 8) | message->buffer[5];

            printf("vfs_server: VFS_NEW_PROCESS called (pid %d, creator %d, flags 0x%x)\n", new_pid, creator_pid, message->buffer[1]);

            result = set_up_filesystem_for_process(state, creator_pid, new_pid, message->buffer[1], FD_REPLY_ENDPOINT(*message).address);

            if (result != 0) {
                printf("vfs_server: set_up_filesystem_for_process failed with error %d\n", result);

                FD_RETURN_VALUE(reply) = result;

                syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);
            }
        }

        break;
    default:
        FD_RETURN_VALUE(reply) = EBADMSG;
        syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);
    }
}

/// the main loop of the vfs server, separated from _start to allow for easier testing and to allow for worker threads to be more easily implemented
STATIC_TESTABLE void main_loop(const struct state *state) {
    struct ipc_message received;

    for (int i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
        received.capabilities[i].address = THREAD_STORAGE_SLOT(state->thread_id, i);
        received.capabilities[i].depth = THREAD_STORAGE_SLOT_DEPTH;
    }

    while (1) {
        size_t result = syscall_invoke(state->endpoint_address, -1, ENDPOINT_RECEIVE, (size_t) &received);

        if (result != 0) {
#ifdef UNDER_TEST
            // like in initrd_fs, there needs to be a way to exit the main loop if this program is being tested, hence the break here
            break;
#else
            printf("vfs_server: endpoint_receive failed with error %d\n", result);
            continue; // TODO: should this really continue? is this actually correct behavior?
#endif
        }

        // TODO: hand off received messages to worker threads with maybe some kind of timeout so they can be killed if a process or fs server is unresponsive
        // TODO: permissions checks in open calls

        if (IPC_FLAGS(received.badge) == IPC_FLAG_IS_DIRECTORY) {
            // handle directory proxy calls
            handle_directory_message(state, &received);
        } else if (IPC_FLAGS(received.badge) == IPC_FLAG_IS_MOUNT_POINT) {
            // handle mount point directory calls
            handle_mount_point_message(state, &received);
        } else {
            // handle vfs calls
            // TODO: have this only be used for process server <-> vfs server communication
            handle_vfs_message(state, &received);
        }

        // delete any leftover capabilities that were transferred
        for (size_t i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
            if ((received.transferred_capabilities & (1 << i)) != 0) {
                syscall_invoke(THREAD_STORAGE_ADDRESS(state->thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, i);
            }
        }
    }
}

void _start(void) {
    printf("hellorld from vfs server!\n");

    // sanity check :3
#ifdef UNDER_TEST
    TEST_ASSERT(sizeof(struct stat) <= IPC_BUFFER_SIZE - sizeof(size_t));
#else
    if (sizeof(struct stat) > IPC_BUFFER_SIZE - sizeof(size_t)) {
        syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "vfs_server: fatal: stat structure is too big!\n");
        while (1);
    }
#endif

    init_vfs_structures();

    // allocate a new endpoint to handle all client <-> vfs communication
    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    const struct state state = {
        .thread_id = 0,
        .temp_slot = IPC_CAPABILITY_SLOTS + 1,
        .endpoint_address = endpoint_alloc_args.address
    };

    set_up_filesystem_for_process(&state, 1, 1, 0, 2);
    main_loop(&state);
}
