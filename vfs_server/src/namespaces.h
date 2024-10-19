#pragma once

#include <stddef.h>
#include "sys/limits.h"
#include "sys/types.h"

/// sets up process data and a filesystem namespace for a new process, then sends the endpoint it will use to communicate with the vfs
size_t set_up_filesystem_for_process(pid_t creator_pid, pid_t new_pid, uint8_t flags, size_t reply_address, size_t endpoint_address, size_t thread_id, size_t slot);

/// mounts the provided file descriptor for a directory into a filesystem's namespace
size_t mount(size_t fs_id, size_t path, size_t directory_fd, uint8_t flags);

/// \brief finds the mount point that matches the given inode and enclosing filesystem.
///
/// upon success, the address in capability space of the mount point will be returned. on failure, -1 will be returned
size_t find_mount_point(size_t fs_id, ino_t inode, size_t enclosing_filesystem);