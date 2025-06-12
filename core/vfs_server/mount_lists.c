#include "errno.h"
#include "mount_lists.h"
#include "structures.h"
#include "sys/vfs.h"

size_t create_mounted_list_entry() {
    size_t info_address = alloc_structure(USED_MOUNTED_LISTS_SLOT, MOUNTED_LIST_INFO_SLOT, MAX_MOUNTED_FS, sizeof(struct mounted_list_info));

    if (info_address == -1) {
        return -1;
    }

    // info struct needs to be initialized, it's easiest to do this before everything else since it makes freeing structures in case of errors easier
    struct mounted_list_info *info = (struct mounted_list_info *) syscall_invoke(info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        free_structure(USED_MOUNTED_LISTS_SLOT, MOUNTED_LIST_INFO_SLOT, MAX_MOUNTED_FS, info_address);
        return -1;
    }

    info->used_slots = 0;
    info->create_flagged_slots = 0;
    info->next_index = -1;

    syscall_invoke(info_address, -1, UNTYPED_UNLOCK, 0);

    size_t mounted_list_index = info_address >> INIT_NODE_DEPTH;

    const struct alloc_args alloc_args = {
        .type = TYPE_NODE,
        .size = MOUNTED_LIST_ENTRY_BITS,
        .address = MOUNTED_LIST_NODE_ADDRESS(mounted_list_index),
        .depth = -1
    };

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) != 0) {
        free_structure(USED_MOUNTED_LISTS_SLOT, MOUNTED_LIST_INFO_SLOT, MAX_MOUNTED_FS, info_address);
        return -1;
    }

    return mounted_list_index;
}

size_t mounted_list_insert(size_t index, size_t directory_fd, uint8_t mount_flags) {
    size_t next_index = -1;
    size_t result = EUNKNOWN;

    while (index != -1) {
        struct mounted_list_info *info = (struct mounted_list_info *) syscall_invoke(MOUNTED_LIST_INFO_ADDRESS(index), -1, UNTYPED_LOCK, 0);

        if (info == NULL) {
            return ENOMEM;
        }

        if (info->used_slots == -1) {
            next_index = info->next_index;

            if (next_index == -1) {
                next_index = info->next_index = create_mounted_list_entry();
                result = ENOMEM; // if this call fails, the loop will be broken out of and this value will be returned
            }
        } else {
            next_index = -1; // this is set to automatically break out of the loop since there's room in this node
            result = 0;

            int slot_index;
            for (slot_index = 0; slot_index < sizeof(size_t) * 8 && (info->used_slots & (1 << slot_index)) != 0; slot_index ++);

            const struct node_move_args move_args = {
                .source_address = directory_fd,
                .source_depth = -1,
                .dest_slot = slot_index
            };

            result = syscall_invoke(MOUNTED_LIST_NODE_ADDRESS(index), MOUNTED_LIST_NODE_DEPTH, NODE_MOVE, (size_t) &move_args);

            if (result == 0) {
                info->used_slots |= (1 << slot_index);

                if (mount_flags & MOUNT_CREATE) {
                    info->create_flagged_slots |= (1 << slot_index);
                }
            }
        }

        syscall_invoke(MOUNTED_LIST_INFO_ADDRESS(index), -1, UNTYPED_UNLOCK, 0);
        index = next_index;
    }

    return result;
}

size_t iterate_over_mounted_list(size_t index, void *data, bool (*fn)(void *, size_t, bool, size_t *)) {
    size_t next_index = -1;

    while (index != -1) {
        struct mounted_list_info *info = (struct mounted_list_info *) syscall_invoke(MOUNTED_LIST_INFO_ADDRESS(index), -1, UNTYPED_LOCK, 0);
        next_index = info->next_index;

        if (info == NULL) {
            return ENOMEM;
        }

        for (int i = 0; i < sizeof(size_t) * 8; i ++) {
            if ((info->used_slots & (1 << i)) == 0) {
                continue;
            }

            bool is_create_flagged = (info->create_flagged_slots & (1 << i)) != 0;
            size_t result = 0;

            if (!fn(data, MOUNTED_LIST_SLOT(index, i), is_create_flagged, &result)) {
                syscall_invoke(MOUNTED_LIST_INFO_ADDRESS(index), -1, UNTYPED_UNLOCK, 0);
                return result;
            }
        }

        syscall_invoke(MOUNTED_LIST_INFO_ADDRESS(index), -1, UNTYPED_UNLOCK, 0);
        index = next_index;
    }

    return ENOENT;
}
