#include "capabilities_layout.h"
#include "core_io.h"
#include "directories.h"
#include "inttypes.h"
#include "ipc.h"
#include "mount_points.h"
#include "namespaces.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "structures.h"
#include "sys/kernel.h"
#include "sys/vfs.h"

/// allocates a new filesystem namespace and returns its address in capability space
static size_t alloc_namespace(void) {
    size_t address = alloc_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, sizeof(struct fs_namespace));

    if (address == SIZE_MAX) {
        return SIZE_MAX;
    }

    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        free_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, address);
        return SIZE_MAX;
    }

    namespace->references = 1;
    namespace->root_address = SIZE_MAX;

    for (int i = 0; i < NUM_BUCKETS; i ++) {
        namespace->mount_point_addresses[i] = SIZE_MAX;
    }

    syscall_invoke(address, SIZE_MAX, UNTYPED_UNLOCK, 0);

    return address;
}

/// frees a filesystem namespace given its id if it has no more references
static void free_namespace(size_t namespace_id) {
    size_t address = (namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        debug_printf("free_namespace: couldn't lock namespace, reference count will be wrong!\n");
        return;
    }

    bool should_free = false;

    if (namespace->references <= 1) {
        should_free = true;
    } else {
        namespace->references --;
    }

    syscall_invoke(address, SIZE_MAX, UNTYPED_UNLOCK, 0);

    if (should_free) {
        free_structure(USED_NAMESPACES_SLOT, NAMESPACE_NODE_SLOT, MAX_NAMESPACES, address);
    }
}

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

    for (unsigned int i = 0; i < sizeof(size_t); i ++) {
        result = permutation_table[result ^ *(data ++)];
    }

    return result;
}

size_t set_up_filesystem_for_process(const struct state *state, pid_t creator_pid, pid_t new_pid, uint8_t flags, size_t reply_address) {
    size_t fs_namespace;
    bool force_disable_namespace_modification = false;

    if ((flags & VFS_SHARE_NAMESPACE) != 0) {
        // use the namespace id of the creator process
        size_t creator_process_data_address = ((size_t) creator_pid << INIT_NODE_DEPTH) | PROCESS_DATA_NODE_SLOT;
        struct process_data *creator_process_data = (struct process_data *) syscall_invoke(creator_process_data_address, SIZE_MAX, UNTYPED_LOCK, 0);

        if (creator_process_data == NULL) {
            return ENOMEM;
        }

        fs_namespace = creator_process_data->fs_namespace;
        force_disable_namespace_modification = !creator_process_data->can_modify_namespace;

        syscall_invoke(creator_process_data_address, SIZE_MAX, UNTYPED_UNLOCK, 0);

        size_t address = (fs_namespace << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
        struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(address, SIZE_MAX, UNTYPED_LOCK, 0);

        if (namespace == NULL) {
            return ENOMEM;
        }

        namespace->references ++;

        syscall_invoke(address, SIZE_MAX, UNTYPED_UNLOCK, 0);
    } else {
        // allocate a new namespace for this process
        size_t namespace_address = alloc_namespace();

        if (namespace_address == SIZE_MAX) {
            return ENOMEM;
        }

        fs_namespace = namespace_address >> INIT_NODE_DEPTH;
    }

    // allocate a new process data structure
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = sizeof(struct process_data),
        .address = ((size_t) new_pid << INIT_NODE_DEPTH) | PROCESS_DATA_NODE_SLOT,
        .depth = SIZE_MAX
    };

    if (syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) != 0) {
        free_namespace(fs_namespace);
        return ENOMEM;
    }

    struct process_data *process_data = (struct process_data *) syscall_invoke(alloc_args.address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (process_data == NULL) {
        free_namespace(fs_namespace);
        syscall_invoke(PROCESS_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, (size_t) new_pid);
        return ENOMEM;
    }

    process_data->fs_namespace = fs_namespace;

    if (force_disable_namespace_modification) {
        process_data->can_modify_namespace = false;
    } else {
        process_data->can_modify_namespace = (flags & VFS_READ_ONLY_NAMESPACE) != 0 ? false : true;
    }

    size_t badge = IPC_BADGE(process_data->fs_namespace, process_data->can_modify_namespace ? IPC_FLAG_CAN_MODIFY : 0);

    syscall_invoke(alloc_args.address, SIZE_MAX, UNTYPED_UNLOCK, 0);

    struct ipc_message message = {
        .capabilities = {{reply_address, SIZE_MAX}},
        .badge = badge
    };
    size_t result = open_root(state, &message, fs_namespace);

    if (result != 0) {
        debug_printf("set_up_filesystem_for_process: open_root failed with error %" PRIdPTR "\n", result);
        free_namespace(fs_namespace);
        syscall_invoke(PROCESS_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, (size_t) new_pid);
    }

    return result;
}

size_t add_mount_point_to_namespace(struct fs_namespace *namespace, struct mount_point *mount_point) {
    size_t mount_point_address = alloc_structure(USED_MOUNT_POINT_IDS_SLOT, MOUNT_POINTS_NODE_SLOT, MAX_MOUNT_POINTS, sizeof(struct mount_point));

    if (mount_point_address == SIZE_MAX) {
        return ENOMEM;
    }

    struct mount_point *new_mount_point = (struct mount_point *) syscall_invoke(mount_point_address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (new_mount_point == NULL) {
        free_structure(USED_MOUNT_POINT_IDS_SLOT, MOUNT_POINTS_NODE_SLOT, MAX_MOUNT_POINTS, mount_point_address);
        return ENOMEM;
    }

    memcpy(new_mount_point, mount_point, sizeof(struct mount_point));

    if (namespace->root_address == SIZE_MAX) {
        // if the root address of this namespace hasn't been set yet, there's no way for this mount call to be mounting anything but the root filesystem.
        // any existing file descriptors for the previously unset root directory will be automatically updated to the new value when operations are performed on them
        namespace->root_address = mount_point_address;
    } else {
        // TODO: should the hash value take something else into account as well (maybe the containing filesystem?) in order to reduce
        // collisions between, for example, sequentially assigned inodes in multiple basic filesystem implementations?
        // or should that be left up to the filesystem drivers themselves
        uint8_t bucket = hash(new_mount_point->inode) % NUM_BUCKETS;
        size_t *bucket_value = &namespace->mount_point_addresses[bucket];

        if (*bucket_value == SIZE_MAX) {
            *bucket_value = mount_point_address;
        } else {
            struct mount_point *other_mount_point = (struct mount_point *) syscall_invoke(*bucket_value, SIZE_MAX, UNTYPED_LOCK, 0);

            if (other_mount_point == NULL) {
                free_structure(USED_MOUNT_POINT_IDS_SLOT, MOUNT_POINTS_NODE_SLOT, MAX_MOUNT_POINTS, mount_point_address);
                return ENOMEM;
            }

            other_mount_point->previous = mount_point_address;

            syscall_invoke(*bucket_value, SIZE_MAX, UNTYPED_UNLOCK, 0); // TODO: is unlocking it here ok? could this cause problems?

            new_mount_point->next = *bucket_value;
            *bucket_value = mount_point_address;
        }
    }

    syscall_invoke(mount_point_address, SIZE_MAX, UNTYPED_UNLOCK, 0);

    return 0;
}

size_t find_mount_point(size_t namespace_id, ino_t inode, size_t enclosing_filesystem) {
    size_t namespace_address = (namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        return SIZE_MAX;
    }

    uint8_t bucket = hash(inode) % NUM_BUCKETS;
    size_t *bucket_value = &namespace->mount_point_addresses[bucket];

    if (*bucket_value == SIZE_MAX) {
        syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_UNLOCK, 0);
        return SIZE_MAX;
    }

    for (size_t address = *bucket_value; address != SIZE_MAX;) {
        struct mount_point *mount_point = (struct mount_point *) syscall_invoke(address, SIZE_MAX, UNTYPED_LOCK, 0);

        if (mount_point == NULL) {
            syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_UNLOCK, 0);
            return SIZE_MAX;
        }

        if (mount_point->inode == inode && mount_point->enclosing_filesystem == enclosing_filesystem) {
            syscall_invoke(address, SIZE_MAX, UNTYPED_UNLOCK, 0);
            syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_UNLOCK, 0);
            return address;
        }

        size_t next_address = mount_point->next;
        syscall_invoke(address, SIZE_MAX, UNTYPED_UNLOCK, 0);
        address = next_address;
    }

    syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_UNLOCK, 0);
    return SIZE_MAX;
}
