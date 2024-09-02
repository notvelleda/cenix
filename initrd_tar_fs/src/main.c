#include "sys/kernel.h"
#include "sys/vfs.h"

#define VFS_ENDPOINT_ADDRESS 2

void _start(void) {
    syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "hellorld from initrd_tar_fs!\n");

    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    struct alloc_args path_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = 1,
        .address = 4,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &path_alloc_args);

    char *pointer = (char *) syscall_invoke(path_alloc_args.address, path_alloc_args.depth, UNTYPED_LOCK, 0);
    *pointer = '/';
    syscall_invoke(path_alloc_args.address, path_alloc_args.depth, UNTYPED_UNLOCK, 0);

    struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 5,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args);

    vfs_mount(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, path_alloc_args.address, fd_alloc_args.address, MREPL);
    //vfs_mount(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, -1, -1, MREPL);

    /*struct ipc_message to_send = {
        .buffer = {VFS_MOUNT, MREPL},
        .capabilities = {{endpoint_alloc_args.address, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    vfs_call(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, &to_send, &to_receive);*/

    syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "got here\n");

    while (1) {
        syscall_yield();
    }
}
