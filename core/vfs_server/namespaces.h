#pragma once

#include "mount_points.h"
#include <stddef.h>
#include <stdint.h>
#include "structures.h"
#include "sys/types.h"

#define NUM_BUCKETS 128

struct fs_namespace {
    /// how many references exist to this namespace
    size_t references;
    /// address of the root mount point in capability space
    size_t root_address;
    /// hash table containing addresses of mount points in capability space
    size_t mount_point_addresses[NUM_BUCKETS];
};

/// sets up process data and a filesystem namespace for a new process, then sends the endpoint it will use to communicate with the vfs
size_t set_up_filesystem_for_process(const struct state *state, pid_t creator_pid, pid_t new_pid, uint8_t flags, size_t reply_address);

/// \brief adds a mount point to a given namespace.
///
/// this will allocate a new mount point structure, meaning the contents of the provided mount point structure are copied into it
size_t add_mount_point_to_namespace(struct fs_namespace *namespace, struct mount_point *mount_point);

/// \brief finds the mount point that matches the given inode and enclosing filesystem.
///
/// upon success, the address in capability space of the mount point will be returned. on failure, -1 will be returned
size_t find_mount_point(size_t namespace_id, ino_t inode, size_t enclosing_filesystem);
