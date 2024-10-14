#pragma once

#include <stdbool.h>
#include <stddef.h>

#define INIT_NODE_DEPTH 4

#define PROCESS_DATA_NODE_SLOT 5
#define MOUNT_POINTS_NODE_SLOT 6
#define USED_MOUNT_POINT_IDS_SLOT 7
#define INODES_SLOT 8
#define USED_INODES_SLOT 9
#define NAMESPACE_NODE_SLOT 10
#define USED_NAMESPACES_SLOT 11

#define MOUNT_POINTS_BITS 8
#define MAX_MOUNT_POINTS 256

#define MOUNTED_FS_BITS 8
#define MAX_MOUNTED_FS 256

#define NAMESPACE_BITS 8
#define MAX_NAMESPACES 256

#define SIZE_BITS (sizeof(size_t) * 8)

/// structure that describes a mount/bind point in the virtual filesystem
struct mount_point {
    /// capability space address of the previous mount point in the list
    size_t previous;
    /// capability space address of the next mount point in the list
    size_t next;
    /// how many references to this mount point exist
    size_t references;
    /// the filesystem that this mount point is contained in
    size_t enclosing_file_system;
    /// the inode (unique identifier) of the directory that this mount point refers to
    // TODO: should inodes always be a size_t? would it be better for them to be a uint32_t so that 8/16 bit systems don't have issues?
    size_t inode;
    /// the address in capability space of the first capability node containing directory entries mounted at this mount point
    size_t first_node;
};

/// structure that describes a list of filesystems mounted on a given mount point
struct mounted_list_info {
    /// stores which slots in the capability node containing this structure are occupied
    size_t used_slots;
    /// stores which slots in the capability node containing this structure are marked with the MCREATE flag
    size_t create_flagged_slots;
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

/// initializes and allocates vfs structures
void init_vfs_structures(void);

size_t alloc_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_size);
size_t free_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_address);
