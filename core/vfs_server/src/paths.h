#pragma once

#include <stddef.h>

size_t traverse_path(size_t path_address, void *data, size_t (*fn)(void *, size_t, const char *));
