#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// \brief creates and initializes a new mounted file descriptor list entry.
///
/// this can be used to create a new mounted file descriptor list, add another link to an existing list, etc.
size_t create_mounted_list_entry();

/// inserts a directory into the given mounted file descriptor list, allocating more links in the list as needed
size_t mounted_list_insert(size_t index, size_t directory_fd, uint8_t mount_flags);

/// \brief helper function to iterate over all file descriptors in a mounted file descriptor list and run a callback over each.
///
/// this callback is passed the value of the `data` argument, the address of the file descriptor,
/// whether this file descriptor was flagged with MOUNT_CREATE, and a pointer to a value that will be returned if the function returns `false`.
/// if it returns `true`, it will continue on to the next file descriptor, call the callback again, and so on and so forth
size_t iterate_over_mounted_list(size_t index, void *data, bool (*fn)(void *, size_t, bool, size_t *));
