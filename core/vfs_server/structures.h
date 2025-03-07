#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "sys/kernel.h"
#include "sys/vfs.h"

#define PROCESS_DATA_NODE_SLOT 4

#define MOUNT_POINTS_NODE_SLOT 5
#define USED_MOUNT_POINT_IDS_SLOT 6

#define MOUNTED_LIST_INFO_SLOT 7
#define MOUNTED_LIST_NODE_SLOT 8
#define USED_MOUNTED_LISTS_SLOT 9

#define NAMESPACE_NODE_SLOT 10
#define USED_NAMESPACES_SLOT 11

#define DIRECTORY_NODE_SLOT 12
#define DIRECTORY_INFO_SLOT 13
#define USED_DIRECTORY_IDS_SLOT 14

#define THREAD_STORAGE_NODE_SLOT 15
// no more slots are available in the root node as it's only 4 bits

#define MOUNT_POINTS_BITS 8
#define MAX_MOUNT_POINTS 256

#define MOUNTED_FS_BITS 8
#define MAX_MOUNTED_FS 256

#define NAMESPACE_BITS 8
#define MAX_NAMESPACES 256

#define DIRECTORY_BITS 8
#define MAX_OPEN_DIRECTORIES 256

// should this be defined here?
#define THREAD_STORAGE_BITS 3 // number of bits required to store IPC_CAPABILITY_SLOTS slots + 1
#define MAX_WORKER_THREADS 8
#define THREAD_STORAGE_NODE_BITS 3 // number of bits required to store MAX_WORKER_THREADS slots

#define SIZE_BITS (sizeof(size_t) * 8)

#define THREAD_STORAGE_ADDRESS(thread_id) (((thread_id) << INIT_NODE_DEPTH) | THREAD_STORAGE_NODE_SLOT)
#define THREAD_STORAGE_DEPTH (THREAD_STORAGE_BITS + INIT_NODE_DEPTH)
#define THREAD_STORAGE_SLOT(thread_id, slot) (((slot) << (THREAD_STORAGE_BITS + INIT_NODE_DEPTH)) | THREAD_STORAGE_ADDRESS(thread_id))
#define THREAD_STORAGE_SLOT_DEPTH (THREAD_STORAGE_NODE_BITS + THREAD_STORAGE_DEPTH)

#define REPLY_ENDPOINT_SLOT ((1 << THREAD_STORAGE_BITS) - 1) // the slot number of the reply endpoint to use when issuing ipc calls to filesystem servers

/// structure that describes a mount/bind point in the virtual filesystem
struct mount_point {
    /// capability space address of the previous mount point in the list
    size_t previous;
    /// capability space address of the next mount point in the list
    size_t next;
    /// how many references to this mount point exist
    size_t references;
    /// the filesystem that this mount point is contained in
    size_t enclosing_filesystem;
    /// the inode (unique identifier) of the directory that this mount point refers to
    ino_t inode;
    /// the address in capability space of the first capability node containing directory entries mounted at this mount point
    size_t first_node;
};

/// structure that describes a list of filesystems mounted on a given mount point
struct mounted_list_info {
    /// stores which slots in the capability node containing this structure are occupied
    size_t used_slots;
    /// stores which slots in the capability node containing this structure are marked with the MCREATE flag
    size_t create_flagged_slots;
    // TODO: should this be part of a linked list so that more mount points than size_t has bits can be added to a given inode?
    // i was originally thinking of something like this after all
};

struct process_data {
    /// the address of the filesystem namespace this process uses
    size_t fs_namespace;
    /// whether this process has permission to modify its namespace
    bool can_modify_namespace;
};

#define NUM_BUCKETS 128

struct fs_namespace {
    /// how many references exist to this namespace
    size_t references;
    /// address of the root mount point in capability space
    size_t root_address;
    /// hash table containing addresses of mount points in capability space
    size_t mount_point_addresses[NUM_BUCKETS];
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
