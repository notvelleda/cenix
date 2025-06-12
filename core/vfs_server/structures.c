#include "assert.h"
#include "capabilities_layout.h"
#include "errno.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/limits.h"

// fills an allocated region of memory with all zeroes
static void clear_allocation(size_t address) {
    size_t size = syscall_invoke(address, -1, UNTYPED_SIZEOF, 0);

    uint8_t *pointer = (uint8_t *) syscall_invoke(address, -1, UNTYPED_LOCK, 0);
    assert(pointer != NULL);

    memset(pointer, 0, size);

    assert(syscall_invoke(address, -1, UNTYPED_UNLOCK, 0) == 0);
}

void init_vfs_structures(void) {
    const struct alloc_args process_data_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PROCESS_DATA_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &process_data_node_alloc_args) == 0);

    const struct alloc_args mount_points_node_alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNT_POINTS_BITS,
        .address = MOUNT_POINTS_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &mount_points_node_alloc_args) == 0);

    const struct alloc_args used_mount_point_ids_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_MOUNT_POINTS / 8,
        .address = USED_MOUNT_POINT_IDS_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_mount_point_ids_alloc_args) == 0);
    clear_allocation(used_mount_point_ids_alloc_args.address);

    const struct alloc_args mounted_list_info_alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNTED_FS_BITS,
        .address = MOUNTED_LIST_INFO_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &mounted_list_info_alloc_args) == 0);

    const struct alloc_args mounted_list_node_alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNTED_FS_BITS,
        .address = MOUNTED_LIST_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &mounted_list_node_alloc_args) == 0);

    const struct alloc_args used_mounted_lists_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_MOUNTED_FS / 8,
        .address = USED_MOUNTED_LISTS_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_mounted_lists_alloc_args) == 0);
    clear_allocation(used_mounted_lists_alloc_args.address);

    const struct alloc_args namespaces_node_alloc_args = {
        .type = TYPE_NODE,
        .size = NAMESPACE_BITS,
        .address = NAMESPACE_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &namespaces_node_alloc_args) == 0);

    const struct alloc_args used_namespaces_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_NAMESPACES / 8,
        .address = USED_NAMESPACES_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_namespaces_alloc_args) == 0);
    clear_allocation(used_namespaces_alloc_args.address);

    const struct alloc_args directory_node_alloc_args = {
        .type = TYPE_NODE,
        .size = DIRECTORY_BITS,
        .address = DIRECTORY_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &directory_node_alloc_args) == 0);

    const struct alloc_args directory_info_alloc_args = {
        .type = TYPE_NODE,
        .size = DIRECTORY_BITS,
        .address = DIRECTORY_INFO_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &directory_info_alloc_args) == 0);

    const struct alloc_args used_directories_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = MAX_OPEN_DIRECTORIES / 8,
        .address = USED_DIRECTORY_IDS_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &used_directories_alloc_args) == 0);
    clear_allocation(used_directories_alloc_args.address);

    const struct alloc_args thread_storage_node_alloc_args = {
        .type = TYPE_NODE,
        .size = THREAD_STORAGE_NODE_BITS,
        .address = THREAD_STORAGE_NODE_SLOT,
        .depth = -1
    };
    assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &thread_storage_node_alloc_args) == 0);

    for (int i = 0; i < MAX_WORKER_THREADS; i ++) {
        const struct alloc_args node_alloc_args = {
            .type = TYPE_NODE,
            .size = 3,
            .address = (i << INIT_NODE_DEPTH) | THREAD_STORAGE_NODE_SLOT,
            .depth = -1
        };
        assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args) == 0);

        const struct alloc_args reply_endpoint_alloc_args = {
            .type = TYPE_ENDPOINT,
            .size = 0,
            .address = (REPLY_ENDPOINT_SLOT << (THREAD_STORAGE_NODE_BITS + INIT_NODE_DEPTH)) | node_alloc_args.address,
            .depth = -1
        };

        assert(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_endpoint_alloc_args) == 0);
    }
}

size_t find_slot_for(size_t used_slots_address, size_t max_items, void *data, size_t (*fn)(void *, size_t)) {
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

    size_t result = fn(data, id);

    if (result == -1) {
        // mark the id as free if the function fails
        pointer[id / SIZE_BITS] &= ~(1 << (id % SIZE_BITS));
    }

    syscall_invoke(used_slots_address, -1, UNTYPED_UNLOCK, 0);

    return result;
}

size_t mark_slot_unused(size_t used_slots_address, size_t max_items, size_t slot_number) {
    size_t *pointer = (size_t *) syscall_invoke(used_slots_address, -1, UNTYPED_LOCK, 0);

    if (pointer == NULL) {
        return ENOMEM;
    }

    if (slot_number >= max_items) {
        size_t unlock_result = syscall_invoke(used_slots_address, -1, UNTYPED_UNLOCK, 0);
        return unlock_result != 0 ? unlock_result : 1;
    }

    pointer[slot_number / SIZE_BITS] &= ~(1 << (slot_number % SIZE_BITS));

    return syscall_invoke(used_slots_address, -1, UNTYPED_UNLOCK, 0);
}

static size_t alloc_structure_callback(void *data, size_t id) {
    struct alloc_args *alloc_args = (struct alloc_args *) data;
    alloc_args->address |= id << INIT_NODE_DEPTH;

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) alloc_args) == 0) {
        return alloc_args->address;
    } else {
        return -1;
    }
}

size_t alloc_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_size) {
    struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = structure_size,
        .address = /*(id << INIT_NODE_DEPTH) |*/ node_address,
        .depth = -1
    };

    return find_slot_for(used_slots_address, max_items, &alloc_args, &alloc_structure_callback);
}

size_t free_structure(size_t used_slots_address, size_t node_address, size_t max_items, size_t structure_address) {
    size_t slot_number = structure_address >> INIT_NODE_DEPTH;

    size_t result = syscall_invoke(node_address, INIT_NODE_DEPTH, NODE_DELETE, slot_number);

    if (result != 0) {
        return result;
    }

    return mark_slot_unused(used_slots_address, max_items, slot_number);
}
