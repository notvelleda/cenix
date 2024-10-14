#include "structures.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "sys/limits.h"
#include "sys/vfs.h"
#include "debug.h"

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

size_t alloc_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_size) {
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

size_t free_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_address) {
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
