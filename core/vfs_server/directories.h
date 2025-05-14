#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "structures.h"
#include "sys/kernel.h"
#include "sys/types.h"

struct directory_info {
    /// the id of the filesystem namespace this directory is in
    size_t namespace_id;
    /// whether this namespace can be modified or not
    bool can_modify_namespace;
    /// the inode of this directory
    ino_t inode;
    union {
        /// the address of the filesystem that this directory is contained within
        size_t enclosing_filesystem;
        /// the address of the mount point this directory refers to
        size_t mount_point_address;
    };
};

/// opens the requested file, then calls fd_stat on it to check if its inode matches that of a mount point. if it doesn't, the newly opened file
/// is sent back to the calling process. if it does match, a new directory endpoint is created for the root directory of the new filesystem
size_t open_file(const struct state *state, size_t fd_endpoint, struct directory_info *info, struct ipc_message *message);

/// handles receiving a message for a directory file descriptor and replying to it
void handle_directory_message(const struct state *state, struct ipc_message *message);

/// opens the root directory of a process' filesystem namespace
size_t open_root(const struct state *state, struct ipc_message *message, size_t namespace_id);
