#pragma once

#include <stddef.h>
#include "structures.h"
#include "sys/kernel.h"

/// handles receiving a message for a mount point directory file descriptor and replying to it
void handle_mount_point_message(const struct state *state, struct ipc_message *message);
