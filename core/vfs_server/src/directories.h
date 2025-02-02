#pragma once

#include <stddef.h>
#include "sys/kernel.h"

/// handles receiving a message for a directory file descriptor and replying to it
void handle_directory_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot);

/// handles receiving a message for a mount point directory file descriptor and replying to it
void handle_mount_point_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot);

/// opens the root directory of a process' filesystem namespace, handling the VFS_OPEN_ROOT call
size_t open_root(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot, size_t namespace_id);
