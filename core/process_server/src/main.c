#include "debug.h"
#include "jax.h"
#include "processes.h"
#include <stdbool.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/vfs.h"

extern const uint8_t _binary_initrd_jax_start;
extern const uint8_t _binary_initrd_jax_end;

#define VFS_ENDPOINT_SLOT 8

static size_t initrd_fs_registers_callback(struct thread_registers *registers, void *data) {
    size_t *addresses = (size_t *) data;
    struct arguments_data arguments_data;

    start_arguments(&arguments_data, registers, 2, sizeof(size_t) * 2);
    add_argument(&arguments_data, registers, addresses[0], sizeof(size_t));
    add_argument(&arguments_data, registers, addresses[1], sizeof(size_t));

    return 0;
}

void _start(void) {
    printf("hellorld from process server!\n");

    init_process_table();

    size_t initrd_size = (size_t) &_binary_initrd_jax_end - (size_t) &_binary_initrd_jax_start;

    printf("initrd is at 0x%x to 0x%x (%d bytes)\n", &_binary_initrd_jax_start, &_binary_initrd_jax_end, initrd_size);

    printf("starting vfs_server...\n");

    pid_t vfs_pid = allocate_pid();

    // allocate an endpoint that the vfs server will use to send a properly set up endpoint back to the process server
    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 6,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    // set up the vfs server's capability space
    struct alloc_args root_alloc_args = {
        .type = TYPE_NODE,
        .size = INIT_NODE_DEPTH,
        .address = 7,
        .depth = INIT_NODE_DEPTH
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &root_alloc_args);

    struct node_copy_args alloc_copy_args = {
        .source_address = 0,
        .source_depth = -1,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &alloc_copy_args);

    struct node_copy_args debug_copy_args = {
        .source_address = 1,
        .source_depth = -1,
        .dest_slot = 1,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &debug_copy_args);

    struct node_copy_args endpoint_copy_args = {
        .source_address = endpoint_alloc_args.address,
        .source_depth = endpoint_alloc_args.depth,
        .dest_slot = 2,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &endpoint_copy_args);

    struct jax_iterator iter;
    open_jax(&iter, &_binary_initrd_jax_start, &_binary_initrd_jax_end);

    exec_from_initrd(vfs_pid, &iter, "/sbin/vfs_server", root_alloc_args.address, root_alloc_args.depth, NULL, NULL);

    printf("done! (pid %d)\n", vfs_pid);

    // receive the endpoint that will be used for communicating with the vfs server from the vfs server
    struct ipc_message message = {
        .capabilities = {{VFS_ENDPOINT_SLOT, -1}}
    };
    syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &message);

    // test messages just to make sure the vfs main loop is working
    /*struct ipc_message message2 = {
        .capabilities = {}
    };
    syscall_invoke(message.capabilities[0].address, message.capabilities[0].depth, ENDPOINT_SEND, (size_t) &message2);
    syscall_invoke(message.capabilities[0].address, message.capabilities[0].depth, ENDPOINT_SEND, (size_t) &message2);*/

    // TODO: start initrd_jax_fs to mount initrd as root directory, mount /proc, start debug_console for initial stdout (/dev/debug_console?), start service manager
    // TODO: ensure initrd_jax_fs starts in this address space on systems with multiple (when support is added)

    printf("starting initrd_jax_fs...\n");

    pid_t initrd_jax_fs_pid = allocate_pid();

    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &root_alloc_args);
    syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &alloc_copy_args);
    syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &debug_copy_args);

    // TODO: inform vfs of new process and how it relates to its creator (in this case, this process) so it can provide a new endpoint for communication with it

    struct ipc_message to_send = {
        .buffer = {
            VFS_NEW_PROCESS,
            VFS_SHARE_NAMESPACE,
            initrd_jax_fs_pid >> 8, initrd_jax_fs_pid & 0xff,
            0, 1
        },
        .capabilities = {{endpoint_alloc_args.address, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {{(2 << INIT_NODE_DEPTH) | root_alloc_args.address, -1}}
    };

    vfs_call(VFS_ENDPOINT_SLOT, endpoint_alloc_args.address, &to_send, &to_receive);

    open_jax(&iter, &_binary_initrd_jax_start, &_binary_initrd_jax_end);

    size_t addresses[2] = {(size_t) &_binary_initrd_jax_start, (size_t) &_binary_initrd_jax_end};
    exec_from_initrd(initrd_jax_fs_pid, &iter, "/sbin/initrd_jax_fs", root_alloc_args.address, root_alloc_args.depth, initrd_fs_registers_callback, &addresses);

    printf("done! (pid %d)\n", initrd_jax_fs_pid);

    while (1) {
        syscall_yield();
    }
}
