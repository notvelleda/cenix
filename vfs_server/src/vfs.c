#include "vfs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "sys/limits.h"
#include "sys/vfs.h"
#include "debug.h"

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

void init_vfs_structures(void) {
    const struct alloc_args process_data_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PROCESS_DATA_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &process_data_node_alloc_args);

    const struct alloc_args mount_points_node_alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNT_POINTS_BITS,
        .address = MOUNT_POINTS_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &mount_points_node_alloc_args);

    const struct alloc_args used_mount_point_ids_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_MOUNT_POINTS / 8,
        .address = USED_MOUNT_POINT_IDS_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_mount_point_ids_alloc_args);

    // TODO: zero this out

    const struct alloc_args inodes_node_alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNTED_FS_BITS,
        .address = INODES_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &inodes_node_alloc_args);

    const struct alloc_args used_inodes_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_MOUNTED_FS / 8,
        .address = USED_INODES_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_inodes_alloc_args);

    // TODO: zero this out

    const struct alloc_args namespaces_node_alloc_args = {
        .type = TYPE_NODE,
        .size = NAMESPACE_BITS,
        .address = NAMESPACE_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &namespaces_node_alloc_args);

    const struct alloc_args used_namespaces_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_NAMESPACES / 8,
        .address = USED_NAMESPACES_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_namespaces_alloc_args);

    // TODO: zero this out
}

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

#define NUM_BUCKETS 128

struct process_data {
    /// the address of the filesystem namespace this process uses
    size_t fs_namespace;
    /// whether this process has permission to modify its namespace
    bool can_modify_namespace;
};

struct fs_namespace {
    /// how many references exist to this namespace
    size_t references;
    /// hash table containing addresses of mount points in capability space
    size_t mount_point_addresses[NUM_BUCKETS];
};

static const uint8_t permutation_table[256] = {
    14, 132, 225, 182, 47, 210, 44, 79, 83, 145, 5, 135, 192, 162, 205, 91,
    29, 55, 48, 152, 246, 12, 117, 104, 211, 119, 49, 206, 36, 72, 24, 3,
    227, 9, 41, 239, 250, 61, 164, 200, 247, 184, 108, 161, 99, 185, 230, 126,
    96, 70, 163, 170, 77, 15, 43, 81, 76, 187, 52, 180, 178, 80, 86, 253,
    141, 105, 157, 168, 93, 196, 193, 13, 232, 125, 115, 183, 127, 51, 129, 222,
    113, 251, 121, 101, 20, 169, 94, 197, 229, 151, 218, 95, 114, 64, 124, 208,
    147, 0, 27, 103, 50, 189, 252, 238, 107, 173, 130, 235, 106, 33, 167, 30,
    156, 237, 140, 57, 214, 249, 155, 143, 78, 39, 203, 171, 2, 242, 219, 177,
    215, 216, 54, 23, 191, 34, 84, 69, 244, 176, 45, 241, 174, 158, 179, 6,
    153, 65, 149, 68, 85, 195, 112, 234, 42, 25, 58, 254, 102, 92, 100, 82,
    220, 236, 1, 110, 56, 63, 194, 136, 16, 223, 139, 209, 35, 37, 40, 87,
    148, 199, 201, 233, 217, 255, 59, 146, 207, 190, 116, 32, 7, 4, 98, 154,
    97, 134, 60, 46, 38, 138, 22, 204, 175, 172, 17, 73, 133, 142, 10, 213,
    240, 11, 228, 66, 131, 224, 118, 74, 181, 160, 19, 159, 31, 53, 248, 120,
    88, 71, 90, 137, 166, 165, 21, 212, 122, 231, 226, 18, 123, 128, 109, 221,
    144, 186, 28, 243, 75, 26, 188, 67, 8, 89, 202, 111, 150, 198, 245, 62
};

// 8-bit hash function using the pearson hash algorithm
static uint8_t hash(size_t value) {
    const uint8_t *data = (uint8_t *) &value;
    uint8_t result = 0;

    for (int i = 0; i < sizeof(size_t); i ++) {
        result = permutation_table[result ^ *(data ++)];
    }

    return result;
}

static size_t alloc_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_size) {
    // find an empty slot in the node for this structure
    // the lock is held throughout to prevent race conditions even tho it probably doesn't matter too much
    size_t *pointer = (size_t *) syscall_invoke(used_slots_address, -1, UNTYPED_LOCK, 0);

    if (pointer == NULL) {
        return -1;
    }

    size_t id = 0;

    for (int i = 0; i < max_items / SIZE_BITS; i ++, pointer ++) {
        size_t value = *pointer;

        if (value == SIZE_MAX) {
            continue;
        }

        int bit_index;
        for (bit_index = 0; bit_index < sizeof(size_t) * 8 && (value & (1 << bit_index)) != 0; bit_index ++);

        *pointer |= (1 << bit_index);

        id = i * SIZE_BITS + bit_index;
        break;
    }

    // allocate the new structure
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = structure_size,
        .address = (id << INIT_NODE_DEPTH) | node_address,
        .depth = -1
    };

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) != 0) {
        pointer[id / SIZE_BITS] &= ~(1 << (id % SIZE_BITS));
        return -1;
    }

    syscall_invoke(used_slots_address, -1, UNTYPED_UNLOCK, 0);

    return alloc_args.address;
}

static size_t free_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_address) {
    // even tho the possibility of a race condition here doesn't matter much, it's better to hold the lock throughout anyway
    size_t *pointer = (size_t *) syscall_invoke(used_slots_address, -1, UNTYPED_LOCK, 0);

    if (pointer == NULL) {
        return 1;
    }

    size_t id = structure_address >> INIT_NODE_DEPTH;

    size_t result = syscall_invoke(node_address, INIT_NODE_DEPTH, NODE_DELETE, id);

    if (result != 0) {
        return result;
    }

    pointer[id / SIZE_BITS] &= ~(1 << (id % SIZE_BITS));

    return syscall_invoke(used_slots_address, -1, UNTYPED_UNLOCK, 0);
}

/// allocates a new filesystem namespace and returns its address in capability space
static size_t alloc_namespace(void) {
    size_t address = alloc_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, sizeof(struct fs_namespace));

    if (address == -1) {
        return -1;
    }

    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, -1, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        free_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, address);
        return -1;
    }

    namespace->references = 1;

    for (int i = 0; i < NUM_BUCKETS; i ++) {
        namespace->mount_point_addresses[i] = -1;
    }

    syscall_invoke(address, -1, UNTYPED_UNLOCK, 0);

    return address;
}

/// frees a filesystem namespace given its id if it has no more references
static void free_namespace(size_t namespace_id) {
    size_t address = (namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, -1, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        printf("free_namespace: couldn't lock namespace, reference count will be wrong!\n");
        return;
    }

    bool should_free = false;

    if (namespace->references <= 1) {
        should_free = true;
    } else {
        namespace->references --;
    }

    syscall_invoke(address, -1, UNTYPED_UNLOCK, 0);

    if (should_free) {
        free_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, address);
    }
}

size_t set_up_filesystem_for_process(pid_t creator_pid, pid_t new_pid, uint8_t flags, size_t reply_address, size_t endpoint_address, size_t node_address, size_t slot, size_t node_bits) {
    size_t fs_namespace;

    if ((flags & VFS_SHARE_NAMESPACE) != 0) {
        // use the namespace id of the creator process
        size_t creator_process_data_address = (creator_pid << INIT_NODE_DEPTH) | PROCESS_DATA_NODE_SLOT;
        struct process_data *creator_process_data = (struct process_data *) syscall_invoke(creator_process_data_address, -1, UNTYPED_LOCK, 0);

        if (creator_process_data == NULL) {
            return 1;
        }

        fs_namespace = creator_process_data->fs_namespace;

        syscall_invoke(creator_process_data_address, -1, UNTYPED_UNLOCK, 0);

        size_t address = (fs_namespace << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
        struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, -1, UNTYPED_LOCK, 0);

        if (namespace == NULL) {
            return 1;
        }

        namespace->references ++;

        syscall_invoke(address, -1, UNTYPED_UNLOCK, 0);
    } else {
        // allocate a new namespace for this process
        size_t namespace_address = alloc_namespace();

        if (namespace_address == -1) {
            return 1;
        }

        fs_namespace = namespace_address >> INIT_NODE_DEPTH;
    }

    // allocate a new process data structure
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = sizeof(struct process_data),
        .address = (new_pid << INIT_NODE_DEPTH) | PROCESS_DATA_NODE_SLOT,
        .depth = -1
    };

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) != 0) {
        free_namespace(fs_namespace);
        return 1;
    }

    struct process_data *process_data = (struct process_data *) syscall_invoke(alloc_args.address, -1, UNTYPED_LOCK, 0);

    if (process_data == NULL) {
        free_namespace(fs_namespace);
        syscall_invoke(PROCESS_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_pid);
        return 1;
    }

    process_data->fs_namespace = fs_namespace; // TODO: set this
    process_data->can_modify_namespace = (flags & VFS_READ_ONLY_NAMESPACE) != 0 ? false : true;

    size_t badge = (process_data->fs_namespace << 1) | (process_data->can_modify_namespace ? 1 : 0);

    syscall_invoke(alloc_args.address, -1, UNTYPED_UNLOCK, 0);

    // badge the vfs endpoint with the new filesystem id and send it to the waiting process
    const struct node_copy_args copy_args = {
        .source_address = endpoint_address,
        .source_depth = -1,
        .dest_slot = slot,
        .access_rights = -1,
        .badge = badge,
        .should_set_badge = 1
    };
    size_t result = syscall_invoke(node_address, INIT_NODE_DEPTH, NODE_COPY, (size_t) &copy_args);

    if (result != 0) {
        printf("set_up_filesystem_for_process: node_copy failed with error %d\n", result);
        free_namespace(fs_namespace);
        syscall_invoke(PROCESS_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_pid);
        return result;
    }

    struct ipc_message message = {
        .capabilities = {{(copy_args.dest_slot << INIT_NODE_DEPTH) | node_address, INIT_NODE_DEPTH + node_bits}}
    };
    result = syscall_invoke(reply_address, -1, ENDPOINT_SEND, (size_t) &message);

    if (result != 0) {
        free_namespace(fs_namespace);
        syscall_invoke(PROCESS_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_pid);
        syscall_invoke(node_address, INIT_NODE_DEPTH, NODE_DELETE, slot);
        return result;
    }

    return 0;
}

size_t mount(void) {
    // TODO
    return 0;
}
