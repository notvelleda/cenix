#include "sys/kernel.h"
#include "sys/vfs.h"

#define VFS_ENDPOINT_ADDRESS 2

void _start(void) {
    syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "hellorld from initrd_jax_fs!\n");

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

    syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "got here\n");

    vfs_open_root(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, 6);

    syscall_invoke(1, -1, DEBUG_PRINT, (size_t) "got here 2\n");

    while (1) {
        syscall_yield();
    }
}
