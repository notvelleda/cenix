#pragma once

#include "sys/kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"

// plan 9-like vfs operations
// TODO: document this
// TODO: should the function declarations here be moved into libc? are the optimization advantages worth it?

#define VFS_OPEN 0
#define VFS_MOUNT 1
#define VFS_UNMOUNT 2

#define VFS_NEW_PROCESS 255

/// if this flag is set, the new process will share its filesystem namespace with its creator
#define VFS_SHARE_NAMESPACE 1
/// if this flag is set, the new process will not be able to run mount/bind/unmount operations
#define VFS_READ_ONLY_NAMESPACE 2

#define MREPL 0
#define MBEFORE 1
#define MAFTER 2
#define MCREATE 4

static inline size_t vfs_call(size_t endpoint, size_t reply_endpoint, struct ipc_message *to_send, struct ipc_message *to_receive) {
    size_t result = syscall_invoke(endpoint, -1, ENDPOINT_SEND, (size_t) to_send);

    if (result != 0) {
        return result;
    }

    result = syscall_invoke(reply_endpoint, -1, ENDPOINT_RECEIVE, (size_t) to_receive);

    if (result != 0) {
        return result;
    }

    return *(size_t *) &to_receive->buffer;
}

static inline size_t vfs_open(size_t vfs_endpoint, size_t reply_endpoint, size_t path, size_t fd_slot, uint8_t flags, uint8_t mode) {
    struct ipc_message to_send = {
        .buffer = {VFS_OPEN, flags, mode},
        .capabilities = {{reply_endpoint, -1}, {path, -1}},
        .to_copy = 1 // copy the reply endpoint only (TODO: should this copy the path too? does it matter?)
    };
    struct ipc_message to_receive = {
        .capabilities = {{fd_slot, -1}}
    };

    return vfs_call(vfs_endpoint, reply_endpoint, &to_send, &to_receive);
}

static inline size_t vfs_mount(size_t vfs_endpoint, size_t reply_endpoint, size_t path, size_t directory_fd, uint8_t flags) {
    struct ipc_message to_send = {
        .buffer = {VFS_MOUNT, flags},
        .capabilities = {{reply_endpoint, -1}, {path, -1}, {directory_fd, -1}},
        .to_copy = 1 // TODO: should these all be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(vfs_endpoint, reply_endpoint, &to_send, &to_receive);
}

static inline size_t vfs_unmount(size_t vfs_endpoint, size_t reply_endpoint, size_t path, size_t to_unmount) {
    struct ipc_message to_send = {
        .buffer = {VFS_UNMOUNT},
        .capabilities = {{reply_endpoint, -1}, {path, -1}, {to_unmount, -1}},
        .to_copy = 1 // TODO: should these all be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(vfs_endpoint, reply_endpoint, &to_send, &to_receive);
}

#define FD_READ 0
#define FD_READ_FAST 1
#define FD_WRITE 2
#define FD_WRITE_FAST 3
#define FD_STAT 4
#define FD_OPEN 5
#define FD_LINK 6
#define FD_UNLINK 7

static inline size_t fd_read(size_t fd_address, size_t reply_endpoint, size_t read_buffer, size_t size, size_t position) {
    struct ipc_message to_send = {
        .buffer = {FD_READ},
        .capabilities = {{reply_endpoint, -1}, {read_buffer, -1}},
        .to_copy = 3
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    // TODO: make sure this works
    size_t *buffer = (size_t *) (&to_send.buffer + 1);
    buffer[0] = size;
    buffer[1] = position;

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_read_fast(size_t fd_address, size_t reply_endpoint, uint8_t *read_buffer, size_t size, size_t position) {
    if (size > IPC_BUFFER_SIZE - sizeof(size_t)) {
        return 1;
    }

    struct ipc_message to_send = {
        .buffer = {FD_READ_FAST},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    *(size_t *) (&to_send.buffer + 1) = position;

    size_t result = vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);

    if (result != 0) {
        return result;
    }

    memcpy(read_buffer, &to_receive.buffer + sizeof(size_t), size);

    return 0;
}

static inline size_t fd_write(size_t fd_address, size_t reply_endpoint, size_t write_buffer, size_t size, size_t position) {
    struct ipc_message to_send = {
        .buffer = {FD_WRITE},
        .capabilities = {{reply_endpoint, -1}, {write_buffer, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    // TODO: make sure this works
    size_t *buffer = (size_t *) (&to_send.buffer + 1);
    buffer[0] = size;
    buffer[1] = position;

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_write_fast(size_t fd_address, size_t reply_endpoint, const uint8_t *write_buffer, size_t size, size_t position) {
    if (size > IPC_BUFFER_SIZE - 1 - sizeof(size_t)) {
        return 1;
    }

    struct ipc_message to_send = {
        .buffer = {FD_WRITE_FAST},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    *(size_t *) (&to_send.buffer + 1) = position;

    memcpy(&to_send.buffer + 1 + sizeof(size_t), write_buffer, size);

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_stat(size_t fd_address, size_t reply_endpoint, size_t stat_address) {
    struct ipc_message to_send = {
        .buffer = {FD_STAT},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {{stat_address, -1}}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_open(size_t fd_address, size_t reply_endpoint, size_t name_address, size_t fd_slot, uint8_t flags, uint8_t mode) {
    struct ipc_message to_send = {
        .buffer = {FD_OPEN, flags, mode},
        .capabilities = {{reply_endpoint, -1}, {name_address, -1}},
        .to_copy = 1 // copy the reply endpoint only (TODO: should this copy the filename too? does it matter?)
    };
    struct ipc_message to_receive = {
        .capabilities = {{fd_slot, -1}}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_link(size_t fd_address, size_t reply_endpoint, size_t old_address, size_t new_address) {
    struct ipc_message to_send = {
        .buffer = {FD_LINK},
        .capabilities = {{reply_endpoint, -1}, {old_address, -1}, {new_address, -1}},
        .to_copy = 1 // TODO: should the old and new addresses be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

static inline size_t fd_unlink(size_t fd_address, size_t reply_endpoint, size_t name_address) {
    struct ipc_message to_send = {
        .buffer = {FD_UNLINK},
        .capabilities = {{reply_endpoint, -1}, {name_address, -1}},
        .to_copy = 1 // TODO: should the filename be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}
