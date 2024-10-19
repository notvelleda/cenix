#pragma once

#include <stddef.h>
#include "sys/kernel.h"

/// handles receiving a message for a directory file descriptor and replying to it
void handle_directory_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot);
