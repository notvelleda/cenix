#pragma once

#include <stddef.h>
#include "structures.h"

/// simple utility function to make returning a value back to the caller process easier
void return_value(size_t reply_capability, size_t error_code);

/// badges the vfs endpoint with the given badge and sends the new endpoint back to the caller process
size_t badge_and_send(const struct state *state, size_t badge, size_t reply_endpoint_address);
