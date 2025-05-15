#include "assert.h"
#include "core_io.h"
#include "jax.h"
#include "processes.h"
#include <stdbool.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/vfs.h"

extern const uint8_t _binary_initrd_jax_start;
extern const uint8_t _binary_initrd_jax_end;

#define ROOT_NODE_ADDRESS 7
#define VFS_ENDPOINT_SLOT 8
#define ROOT_FD_SLOT 9

/// \brief starts a core/early init process that doesn't require libc.
///
/// the function provided in `setup_callback` is called after the process' root node is set up in order to do any process-specific setup.
/// it takes the PID that will be assigned to the newly created process and the value of `setup_callback_data` as arguments.
///
/// the `registers_callback` and `registers_callback_data` arguments are directly passed to exec_from_initrd()
static void start_process(
    const char *filename,
    void (*setup_callback)(pid_t, void *),
    void *setup_callback_data,
    size_t (*registers_callback)(struct thread_registers *, void *),
    void *registers_callback_data
) {
    puts(" - ");
    puts(filename);
    puts("\n");

    pid_t pid = allocate_pid();

    // set up the new process's capability space
    struct alloc_args root_alloc_args = {
        .type = TYPE_NODE,
        .size = INIT_NODE_DEPTH,
        .address = ROOT_NODE_ADDRESS,
        .depth = INIT_NODE_DEPTH
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &root_alloc_args) == 0);

    struct node_copy_args alloc_copy_args = {
        .source_address = 0,
        .source_depth = -1,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    assert(syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &alloc_copy_args) == 0);

    struct node_copy_args debug_copy_args = {
        .source_address = 1,
        .source_depth = -1,
        .dest_slot = 1,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    assert(syscall_invoke(root_alloc_args.address, root_alloc_args.depth, NODE_COPY, (size_t) &debug_copy_args) == 0);

    // callback is called here to run any additional preparation steps before the process is started
    setup_callback(pid, setup_callback_data);

    struct jax_iterator iter;
    assert(open_jax(&iter, &_binary_initrd_jax_start, &_binary_initrd_jax_end) == true);

    assert(exec_from_initrd(pid, &iter, filename, root_alloc_args.address, root_alloc_args.depth, registers_callback, registers_callback_data) == 0);

    debug_printf("done! (pid %d)\n", pid);
}

void vfs_server_setup_callback(pid_t pid, void *data) {
    struct alloc_args *endpoint_alloc_args = (struct alloc_args *) data;

    struct node_copy_args endpoint_copy_args = {
        .source_address = endpoint_alloc_args->address,
        .source_depth = endpoint_alloc_args->depth,
        .dest_slot = 2,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    assert(syscall_invoke(ROOT_NODE_ADDRESS, INIT_NODE_DEPTH, NODE_COPY, (size_t) &endpoint_copy_args) == 0);
}

void initrd_fs_setup_callback(pid_t pid, void *data) {
    struct alloc_args *endpoint_alloc_args = (struct alloc_args *) data;

    // call VFS_NEW_PROCESS in order to set up this new process in the vfs
    struct ipc_message to_send = {
        .buffer = {
            VFS_NEW_PROCESS,
            VFS_SHARE_NAMESPACE,
            pid >> 8, pid & 0xff,
            0, 1
        },
        .capabilities = {{endpoint_alloc_args->address, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {{(2 << INIT_NODE_DEPTH) | ROOT_NODE_ADDRESS, -1}}
    };

    assert(vfs_call(VFS_ENDPOINT_SLOT, endpoint_alloc_args->address, &to_send, &to_receive) == 0);
}

static size_t initrd_fs_registers_callback(struct thread_registers *registers, void *data) {
    size_t *addresses = (size_t *) data;
    struct arguments_data arguments_data;

    start_arguments(&arguments_data, registers, 2, sizeof(size_t) * 2);
    add_argument(&arguments_data, registers, addresses[0], sizeof(size_t));
    add_argument(&arguments_data, registers, addresses[1], sizeof(size_t));

    return 0;
}

void early_init(void) {
    //size_t initrd_size = (size_t) &_binary_initrd_jax_end - (size_t) &_binary_initrd_jax_start;
    //debug_printf("initrd is at 0x%x to 0x%x (%d bytes)\n", &_binary_initrd_jax_start, &_binary_initrd_jax_end, initrd_size);

    puts("starting core processes:\n");

    // allocate an initial endpoint that the vfs server will use to send a properly set up endpoint back to the process server
    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 6,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args) == 0);

    // start vfs server
    start_process("/lib/core/vfs_server", vfs_server_setup_callback, &endpoint_alloc_args, NULL, NULL);

    // receive the endpoint that will be used for communicating with the vfs server from the vfs server
    const struct ipc_message message = {
        .capabilities = {{VFS_ENDPOINT_SLOT, -1}}
    };
    assert(syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &message) == 0);

    // receive a file descriptor for the root directory of the filesystem
    const struct ipc_message message_2 = {
        .capabilities = {{ROOT_FD_SLOT, -1}}
    };
    assert(syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &message_2) == 0);

    // start initrd_fs now that the vfs server is running. this'll populate the filesystem with a decent initial set of directories (/dev, /proc, etc.)
    // TODO: ensure initrd_fs starts in this address space on systems with multiple (when support is added)
    size_t addresses[2] = {(size_t) &_binary_initrd_jax_start, (size_t) &_binary_initrd_jax_end};
    start_process("/lib/core/initrd_fs", initrd_fs_setup_callback, &endpoint_alloc_args, initrd_fs_registers_callback, &addresses);

    // TODO: start debug_console for initial stdout/stderr (/dev/debug_console?), mount /proc, start early init stage 2 as its own process to start device manager, find/mount a root filesystem,
    // and proceed with initialization from there (should that be in its own stage? or would early init stage 2 suffice and could therefore be renamed)
    // TODO: how should stdin be handled? maybe the same process that does debug_console for stdout can handle stuff like /dev/null and /dev/zero
}
