#pragma once

#include "errno.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "sys/types.h"
#include "sys/stat.h"

// plan 9-like vfs operations
// TODO: document this
// TODO: should the function declarations here be moved into libc? are the optimization advantages worth it?
// TODO: should the position argument (or next_entry_position in vfs_directory_entry, or size in FD_TRUNCATE) really be size_t? shouldn't it be uint32_t/uint64_t
// TODO: properly document that strings in untyped capabilities must be null terminated since the kernel doesn't guarantee that untyped objects will stay the same size
// (though they should be bounds checked with untyped_sizeof anyway)

#define VFS_OPEN 0
#define VFS_MOUNT 1
#define VFS_UNMOUNT 2

#define VFS_NEW_PROCESS 255

/// if this flag is set, the new process will share its filesystem namespace with its creator
#define VFS_SHARE_NAMESPACE 1
/// if this flag is set, the new process will not be able to run mount/bind/unmount operations
#define VFS_READ_ONLY_NAMESPACE 2

#define MREPL 1
#define MBEFORE 2
#define MAFTER 4
#define MCREATE 8
// TODO: should MCACHE exist? is it worth caching things? how should caching even work here

#define MODE_EXEC 1
#define MODE_READ 2
#define MODE_WRITE 4
#define MODE_APPEND 8

#define OPEN_CREATE 1
#define OPEN_EXCLUSIVE 2
#define OPEN_DIRECTORY 4
#define OPEN_NO_FOLLOW 8

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
#define FD_TRUNCATE 8
// TODO: chmod, chown (could these be combined into one call?)

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
        return EINVAL;
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
        return EINVAL;
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

static inline size_t fd_stat(size_t fd_address, size_t reply_endpoint, struct stat *stat_buffer) {
    struct ipc_message to_send = {
        .buffer = {FD_STAT},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    size_t result = vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);

    if (result == 0) {
        memcpy(stat_buffer, &to_receive.buffer[sizeof(size_t)], sizeof(struct stat));
    }

    return result;
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

// TODO: figure out how the Fuck to do this, since in order to make it work there needs to be a way to get the badge of an endpoint if and only if a thread possesses
// the endpoint it originated from
static inline size_t fd_link(size_t fd_address, size_t reply_endpoint, size_t fd_to_link, size_t name_address) {
    struct ipc_message to_send = {
        .buffer = {FD_LINK},
        .capabilities = {{reply_endpoint, -1}, {fd_to_link, -1}, {name_address, -1}},
        .to_copy = 1 // TODO: should the file descriptor to link and name be copied?
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

static inline size_t fd_truncate(size_t fd_address, size_t reply_endpoint, size_t size) {
    struct ipc_message to_send = {
        .buffer = {FD_TRUNCATE},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    *(size_t *) (&to_send.buffer + 1) = size;

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

/// \brief describes the format of directory entries as returned by the VFS and all filesystem servers.
///
/// directory entries are read by opening a directory (i.e. with `vfs_open`) and reading their contents similarly to a regular file.
///
/// however, unlike with character streams, the `position` argument to `fd_read`/`fd_read_fast` is undefined but consistent, with the only defined value being
/// 0 (which always means the first entry in the directory).
/// in order to read directory entries other than the first one, the `position` argument must be set to the value of `next_entry` as read from another valid
/// directory entry (i.e. one which was also read with a valid position) in the same directory.
///
/// if the position at which a directory entry is read is not 0 and has not come from the `next_entry` field of a valid directory entry in that same directory
/// file descriptor, the behavior of any `fd_read`/`fd_read_fast` calls at that position is undefined.
///
/// when reading a directory entry, each call to `fd_read`/`fd_read_fast` will fill up as much of the provided buffer as possible from that position,
/// however unlike with files you cannot increment the position value to read more of the directory entry afterwards (since this would lead to undefined behavior).
/// if more of a directory entry needs to be read, this can be accomplished by simply calling `fd_read`/`fd_read_fast` again at that same position but with a larger
/// buffer.
struct vfs_directory_entry {
    /// \brief the inode number corresponding to this directory entry.
    ///
    /// this must be a unique (to this filesystem) ID corresponding to the file with the name in this directory entry in the directory being read.
    /// hard-linked files (i.e. files sharing an inode) are allowed to share the same ID number.
    ///
    /// this field is effectively meaningless to most programs since the user-facing filesystem API exclusively uses filenames and paths, however
    /// it's crucial to making the VFS layer work properly so it's required.
    ino_t inode;
    /// \brief the position at which to read the next directory entry in this directory.
    ///
    /// if this value is 0, there are no more entries left in this directory.
    /// see the description of the `vfs_directory_entry` struct for further details.
    size_t next_entry_position;
    /// \brief the length of the name of this directory entry.
    ///
    /// the filename is located immediately after this field and must be the length stored in this field without a null terminator at the end.
    uint16_t name_length;
};
