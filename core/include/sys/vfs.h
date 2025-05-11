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

#define VFS_OPEN_ROOT 0

// TODO: replace the vfs call endpoint with a root directory endpoint, replace vfs_mount/vfs_unmount with fd_mount/fd_unmount so path parsing isn't required in the vfs server
// this will likely require optimization to speed up path traversal, maybe a flag could be passed to fd_open to have it open in place? this should be easily doable, would get rid of a decent chunk of overhead,
// and would greatly simplify path traversal in libc

#define VFS_NEW_PROCESS 255

/// if this flag is set, the new process will share its filesystem namespace with its creator
#define VFS_SHARE_NAMESPACE 1
/// if this flag is set, the new process will not be able to run mount/bind/unmount operations
#define VFS_READ_ONLY_NAMESPACE 2

#define MOUNT_REPLACE 1
#define MOUNT_BEFORE 2
#define MOUNT_AFTER 4
#define MOUNT_CREATE 8
// TODO: should MOUNT_CACHE exist? is it worth caching things? how should caching even work here

#define MODE_EXEC 1
#define MODE_READ 2
#define MODE_WRITE 4
#define MODE_APPEND 8

#define OPEN_CREATE 1
#define OPEN_EXCLUSIVE 2
#define OPEN_DIRECTORY 4

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

static inline size_t vfs_open_root(size_t vfs_endpoint, size_t reply_endpoint, size_t fd_slot) {
    struct ipc_message to_send = {
        .buffer = {VFS_OPEN_ROOT},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {{fd_slot, -1}}
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
#define FD_MOUNT 9
#define FD_UNMOUNT 10
// TODO: chmod, chown (could these be combined into one call?)

/// gets a number corresponding to which operation was requested on a file descriptor when given an IPC message
#define FD_CALL_NUMBER(message) ((message).buffer[0])

#define FD_RETURN_VALUE(message) (*(size_t *) (&(message).buffer))
#define FD_REPLY_ENDPOINT(message) ((message).capabilities[0])

#define FD_READ_BUFFER(message) ((message).capabilities[1])
#define FD_READ_SIZE(message) (((size_t *) ((message).buffer + 1))[0])
#define FD_READ_POSITION(message) (((size_t *) ((message).buffer + 1))[1])
#define FD_READ_BYTES_READ(message) (*(size_t *) ((message).buffer + sizeof(size_t)))

static inline size_t fd_read(size_t fd_address, size_t reply_endpoint, size_t read_buffer, size_t size, size_t position, size_t *bytes_read) {
    struct ipc_message to_send = {
        .buffer = {FD_READ},
        .capabilities = {{reply_endpoint, -1}, {read_buffer, -1}},
        .to_copy = 3
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    FD_READ_SIZE(to_send) = size;
    FD_READ_POSITION(to_send) = position;

    size_t result = vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);

    *bytes_read = FD_READ_BYTES_READ(to_receive);

    return result;
}

#define FD_READ_FAST_DATA(message) ((message).buffer + sizeof(size_t) * 2)
#define FD_READ_FAST_SIZE(message) ((message).buffer[1])
#define FD_READ_FAST_MAX_SIZE (IPC_BUFFER_SIZE - sizeof(size_t) * 2)
#define FD_READ_FAST_POSITION(message) (*(size_t *) ((message).buffer + 2))
#define FD_READ_FAST_BYTES_READ(message) (*(size_t *) ((message).buffer + sizeof(size_t)))

static inline size_t fd_read_fast(size_t fd_address, size_t reply_endpoint, uint8_t *read_buffer, size_t size, size_t position, size_t *bytes_read) {
    if (size > FD_READ_FAST_MAX_SIZE) {
        return EINVAL;
    }

    struct ipc_message to_send = {
        .buffer = {FD_READ_FAST, (uint8_t) size},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    FD_READ_FAST_POSITION(to_send) = position;

    size_t result = vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);

    if (result != 0) {
        return result;
    }

    *bytes_read = FD_READ_FAST_BYTES_READ(to_receive);

    memcpy(read_buffer, FD_READ_FAST_DATA(to_receive), size > *bytes_read ? *bytes_read : size);

    return 0;
}

#define FD_WRITE_BUFFER(message) ((message).capabilities[1])
#define FD_WRITE_SIZE(message) (((size_t *) ((message).buffer + 1))[0])
#define FD_WRITE_POSITION(message) (((size_t *) ((message).buffer + 1))[1])
#define FD_WRITE_BYTES_WRITTEN(message) (*(size_t *) ((message).buffer + sizeof(size_t)))

static inline size_t fd_write(size_t fd_address, size_t reply_endpoint, size_t write_buffer, size_t size, size_t position, size_t *bytes_written) {
    struct ipc_message to_send = {
        .buffer = {FD_WRITE},
        .capabilities = {{reply_endpoint, -1}, {write_buffer, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    FD_WRITE_SIZE(to_send) = size;
    FD_WRITE_POSITION(to_send) = position;

    size_t result = vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);

    *bytes_written = FD_WRITE_BYTES_WRITTEN(to_receive);

    return result;
}

#define FD_WRITE_FAST_DATA(message) ((message).buffer + 2 + sizeof(size_t))
#define FD_WRITE_FAST_SIZE(message) ((message).buffer[1])
#define FD_WRITE_FAST_MAX_SIZE (IPC_BUFFER_SIZE - 2 - sizeof(size_t))
#define FD_WRITE_FAST_POSITION(message) (*(size_t *) ((message).buffer + 2))
#define FD_WRITE_FAST_BYTES_WRITTEN(message) (*(size_t *) ((message).buffer + sizeof(size_t)))

static inline size_t fd_write_fast(size_t fd_address, size_t reply_endpoint, const uint8_t *write_buffer, size_t size, size_t position, size_t *bytes_written) {
    if (size > FD_WRITE_FAST_MAX_SIZE) {
        return EINVAL;
    }

    struct ipc_message to_send = {
        .buffer = {FD_WRITE_FAST, (uint8_t) size},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    FD_WRITE_FAST_POSITION(to_send) = position;

    memcpy(FD_WRITE_FAST_DATA(to_send), write_buffer, size);

    *bytes_written = FD_WRITE_FAST_BYTES_WRITTEN(to_receive);

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

#define FD_STAT_STRUCT(message) (*(struct stat *) ((message).buffer + sizeof(size_t)))

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
        memcpy(stat_buffer, &FD_STAT_STRUCT(to_receive), sizeof(struct stat));
    }

    return result;
}

#define FD_OPEN_FLAGS(message) ((message).buffer[1])
#define FD_OPEN_MODE(message) ((message).buffer[2])
#define FD_OPEN_NAME_ADDRESS(message) ((message).capabilities[1])
#define FD_OPEN_REPLY_FD(message) ((message).capabilities[0])

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

#define FD_LINK_FD(message) ((message).capabilities[1])
#define FD_LINK_NAME_ADDRESS(message) ((message).capabilities[2])

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

#define FD_UNLINK_NAME_ADDRESS(message) ((message).capabilities[1])

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

#define FD_TRUNCATE_SIZE(message) (*(size_t *) ((message).buffer + 1))

static inline size_t fd_truncate(size_t fd_address, size_t reply_endpoint, size_t size) {
    struct ipc_message to_send = {
        .buffer = {FD_TRUNCATE},
        .capabilities = {{reply_endpoint, -1}},
        .to_copy = 1
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    FD_TRUNCATE_SIZE(to_send) = size;

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

#define FD_MOUNT_FLAGS(message) ((message).buffer[1])
#define FD_MOUNT_FILE_DESCRIPTOR(message) ((message).capabilities[1])

static inline size_t fd_mount(size_t fd_address, size_t reply_endpoint, size_t directory_fd, uint8_t flags) {
    struct ipc_message to_send = {
        .buffer = {FD_MOUNT, flags},
        .capabilities = {{reply_endpoint, -1}, {directory_fd, -1}},
        .to_copy = 1 // TODO: should both of these be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

#define FD_UNMOUNT_FILE_DESCRIPTOR(message) ((message).capabilities[1])

static inline size_t fd_unmount(size_t fd_address, size_t reply_endpoint, size_t to_unmount) {
    struct ipc_message to_send = {
        .buffer = {FD_UNMOUNT},
        .capabilities = {{reply_endpoint, -1}, {to_unmount, -1}},
        .to_copy = 1 // TODO: should both of these be copied?
    };
    struct ipc_message to_receive = {
        .capabilities = {}
    };

    return vfs_call(fd_address, reply_endpoint, &to_send, &to_receive);
}

/// \brief describes the format of directory entries as returned by the VFS and all filesystem servers.
///
/// directory entries are read by opening a directory (i.e. with `vfs_open_root`/`fd_open`) and reading their contents similarly to a regular file.
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
    /// this field is effectively meaningless to most programs since the user-facing filesystem API exclusively uses filenames, however
    /// it's crucial to making the VFS layer work properly so it's required.
    ino_t inode;
    /// \brief the position at which to read the next directory entry in this directory.
    ///
    /// if this value is 0, there are no more entries left in this directory.
    /// see the description of the `vfs_directory_entry` struct for further details.
    size_t next_entry_position;
    /// \brief the length of the name of this directory entry.
    ///
    /// this describes how long the `name` field immediately following this is in bytes.
    uint16_t name_length;
    /// \brief the name of this directory entry.
    ///
    /// this must be the length stored in the `name_length` field without a null terminator.
    uint8_t name[];
};
