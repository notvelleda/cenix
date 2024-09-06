#pragma once

#include <stddef.h>
#include <stdint.h>
#include "sys/limits.h"
#include "sys/types.h"

/// initializes and allocates vfs structures
void init_vfs_structures(void);

/// sets up process data and a filesystem namespace for a new process, then sends the endpoint it will use to communicate with the vfs
size_t set_up_filesystem_for_process(pid_t creator_pid, pid_t new_pid, uint8_t flags, size_t reply_address, size_t endpoint_address, size_t node_address, size_t slot, size_t node_bits);

size_t mount(void);
