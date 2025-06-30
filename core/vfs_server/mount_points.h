#pragma once

#include "directories.h"
#include <stddef.h>
#include "structures.h"
#include "sys/kernel.h"

/// structure that describes a mount/bind point in the virtual filesystem
struct mount_point {
    /// capability space address of the previous mount point in the list
    size_t previous;
    /// capability space address of the next mount point in the list
    size_t next;
    /// how many references to this mount point exist
    size_t references;
    /// the filesystem that this mount point is contained in
    size_t enclosing_filesystem;
    /// the inode (unique identifier) of the directory that this mount point refers to
    ino_t inode;
    /// the index at which the mounted list info and mounted list can be found for this mount point
    size_t mounted_list_index;
};

#ifdef UNDER_TEST
void mount_point_open(const struct state *state, struct directory_info *info, struct ipc_message *message);
#endif

/// handles receiving a message for a mount point directory file descriptor and replying to it
void handle_mount_point_message(const struct state *state, struct ipc_message *message);

/// mounts the provided file descriptor for a directory into a filesystem's namespace
size_t mount(struct directory_info *info, size_t directory_fd, uint8_t flags);
