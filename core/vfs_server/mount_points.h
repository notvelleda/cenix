#pragma once

#include "directories.h"
#include <stddef.h>
#include "structures.h"
#include "sys/kernel.h"

/// handles receiving a message for a mount point directory file descriptor and replying to it
void handle_mount_point_message(const struct state *state, struct ipc_message *message);

/// mounts the provided file descriptor for a directory into a filesystem's namespace
size_t mount(struct directory_info *info, size_t directory_fd, uint8_t flags);
