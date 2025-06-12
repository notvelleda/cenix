#pragma once

#include <stdbool.h>
#include <stddef.h>

struct process_data {
    /// the address of the filesystem namespace this process uses
    size_t fs_namespace;
    /// whether this process has permission to modify its namespace
    bool can_modify_namespace;
};

/// stores state that gets passed around a lot between function calls to make passing and accessing it cleaner
struct state {
    /// the id of the current worker thread
    size_t thread_id;
    /// the index of an available thread-local storage slot that can be used for temporary purposes
    size_t temp_slot;
    /// the address of the vfs call endpoint
    size_t endpoint_address;
};

/// initializes and allocates vfs structures
void init_vfs_structures(void);

/// \brief finds the index of a cleared bit in a bitset, setting it in the process, and calls the given callback while its lock is held.
///
/// `used_slots_address` denotes the address of the capability containing the bitset, and `max_items` is the maximum number of entries in that bitset.
///
/// the first argument to the callback function is the `data` argument, and the second argument to it is the id of the free bit in the bitset.
/// if the callback returns -1, the bit at the given index will be cleared before the function returns.
size_t find_slot_for(size_t used_slots_address, size_t max_items, void *data, size_t (*fn)(void *, size_t));

/// \brief clears a bit in a bitset.
///
/// `used_slots_address` denotes the address of the capability containing the bitset, and `max_items` is the maximum number of entries in that bitset.
/// `slot_number` is the index of the bit to clear in that bitset, which is bounds checked with `max_items`.
///
/// upon success, 0 is returned. if an error is encountered, the non-zero error value will be returned.
size_t mark_slot_unused(size_t used_slots_address, size_t max_items, size_t slot_number);

size_t alloc_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_size);
size_t free_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_address);
