#include "core_io.h"
#include "directories.h"
#include "errno.h"
#include "ipc.h"
#include "mount_lists.h"
#include "mount_points.h"
#include "namespaces.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/vfs.h"
#include "utils.h"

/// small wrapper to call iterate_over_mounted_list for the mounted list corresponding to a given mount point
static size_t iterate_over_mount_point(size_t mount_point_address, void *data, bool (*fn)(void *, size_t, bool, size_t *)) {
    // mount point lock is held throughout so that nothing wacky happens. not sure if the performance of this is especially Good but whatever
    struct mount_point *mount_point = (struct mount_point *) syscall_invoke(mount_point_address, -1, UNTYPED_LOCK, 0);

    if (mount_point == NULL) {
        return ENOMEM;
    }

    size_t result = iterate_over_mounted_list(mount_point->mounted_list_index, data, fn);

    syscall_invoke(mount_point_address, -1, UNTYPED_UNLOCK, 0);
    return result;
}

/// handles FD_STAT calls for mount points
static void mount_point_stat(struct directory_info *info, struct ipc_message *message) {
    struct ipc_message reply = {
        .capabilities = {}
    };
    FD_RETURN_VALUE(reply) = 0;

    struct stat *stat = &FD_STAT_STRUCT(reply);
    stat->st_dev = info->mount_point_address;
    stat->st_rdev = 0;
    stat->st_size = 0;
    stat->st_blksize = 0;
    stat->st_blocks = 0;

    // TODO: stat the directory this mount point is contained within and preserve these values (what's the most efficient way to do this?)
    stat->st_ino = 0;
    stat->st_mode = 0;
    stat->st_nlink = 0;
    stat->st_uid = 0;
    stat->st_gid = 0;
    stat->st_atime = 0;
    stat->st_mtime = 0;
    stat->st_ctime = 0;

    syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);
}

struct link_args {
    struct ipc_message to_send;
    bool requires_create_flag;
};

/// iterate_over_mount_point() callback to handle FD_LINK/FD_UNLINK for mount points
static bool link_callback(void *data, size_t directory_address, bool is_create_flagged, size_t *result_value) {
    struct link_args *args = (struct link_args *) data;

    if (!is_create_flagged && args->requires_create_flag) {
        // FD_LINK in a union directory should only successfully link in directories marked with MOUNT_CREATE
        return true;
    }

    struct ipc_message to_receive = {
        .capabilities = {}
    };

    // TODO: have a timeout on this
    size_t result = vfs_call(directory_address, FD_REPLY_ENDPOINT(args->to_send).address, &args->to_send, &to_receive);

    if (result == 0) {
        // if the link call succeeded, we're done
        *result_value = 0;
        return false;
    } else {
        return true;
    }
}

/// handles FD_LINK calls in mount points
static void mount_point_link(size_t thread_id, struct directory_info *info, struct ipc_message *message) {
    struct link_args args = {
        .to_send = {
            .buffer = {FD_LINK},
            .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, FD_LINK_FD(*message), FD_LINK_NAME_ADDRESS(*message)},
            .to_copy = 7
        },
        .requires_create_flag = true
    };
    return_value(FD_REPLY_ENDPOINT(*message).address, iterate_over_mount_point(info->mount_point_address, &args, &link_callback));
}

/// handles FD_UNLINK calls in mount points
static void mount_point_unlink(size_t thread_id, struct directory_info *info, struct ipc_message *message) {
    struct link_args args = {
        .to_send = {
            .buffer = {FD_UNLINK},
            .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, FD_UNLINK_NAME_ADDRESS(*message)},
            .to_copy = 3
        },
        .requires_create_flag = false
    };
    return_value(FD_REPLY_ENDPOINT(*message).address, iterate_over_mount_point(info->mount_point_address, &args, &link_callback));
}

struct open_args {
    const struct state *state;
    struct directory_info info;
    struct ipc_message *message;
    bool allow_create;
};

static bool open_callback(void *data, size_t directory_address, bool is_create_flagged, size_t *result_value) {
    struct open_args *args = (struct open_args *) data;

    if (is_create_flagged && !args->allow_create) {
        return true;
    }

    size_t result = open_file(args->state, directory_address, &args->info, args->message);

    if (result == 0) {
        *result_value = 0;
        return false;
    } else {
        return true;
    }
}

/// handles FD_OPEN calls in mount points
static void mount_point_open(const struct state *state, struct directory_info *info, struct ipc_message *message) {
    struct mount_point *mount_point = (struct mount_point *) syscall_invoke(info->mount_point_address, -1, UNTYPED_LOCK, 0);

    if (mount_point == NULL) {
        return_value(FD_REPLY_ENDPOINT(*message).address, ENOMEM);
    }

    struct open_args args = {
        .state = state,
        .info = {
            .namespace_id = info->namespace_id,
            .enclosing_filesystem = mount_point->enclosing_filesystem
        },
        .message = message,
        .allow_create = false
    };
    size_t result;

    if ((FD_OPEN_FLAGS(*message) & (OPEN_CREATE | OPEN_EXCLUSIVE)) == (OPEN_CREATE | OPEN_EXCLUSIVE)) {
        // TODO: fail if a file with this name already exists
        args.allow_create = true;
        result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);
    } else {
        bool should_create = (FD_OPEN_FLAGS(*message) & OPEN_CREATE) != 0;
        FD_OPEN_FLAGS(*message) &= ~OPEN_CREATE;

        syscall_invoke(info->mount_point_address, -1, UNTYPED_UNLOCK, 0);

        result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);

        if (result != 0 && should_create) {
            args.allow_create = true;
            FD_OPEN_FLAGS(*message) |= OPEN_CREATE;
            result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);
        }
    }

    if (result != 0) {
        return_value(FD_REPLY_ENDPOINT(*message).address, result);
    }
}

void handle_mount_point_message(const struct state *state, struct ipc_message *message) {
    size_t info_address = IPC_ID(message->badge); // TODO: should this not just contain the slot number of the structure instead of its full address?

    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        return_value(FD_REPLY_ENDPOINT(*message).address, ENOMEM);
        return;
    }

    if (info->mount_point_address == -1) {
        // if this mount point file descriptor was created before the root filesystem for its namespace was mounted, then it's necessary
        // to check whether the root filesystem has actually been mounted before attempting any operations on this file descriptor.
        // this probably isn't the ideal way of doing it, but it should work and i'm hoping the added overhead on these calls isn't that bad

        size_t namespace_address = (info->namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
        struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, -1, UNTYPED_LOCK, 0);

        if (namespace != NULL) {
            if (namespace->root_address != -1) {
                // way too many levels of indentation here but i'd rather not make this its own function
                info->mount_point_address = namespace->root_address;
            }

            syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0);
        }
    }

    if (info->mount_point_address == -1) {
        debug_printf("vfs_server: got message %d for unset root mount point!\n", FD_CALL_NUMBER(*message));
    } else {
        debug_printf("vfs_server: got message %d for mount point %d!\n", FD_CALL_NUMBER(*message), info->mount_point_address >> INIT_NODE_DEPTH);
    }

    switch (FD_CALL_NUMBER(*message)) {
    case FD_READ:
    case FD_READ_FAST:
        // TODO: this one is a fucking Mess and idk really how to implement it well. all the calls should be intercepted so that the highest position value can be found
        // for each directory in the mount point, so after the end of a directory is found an offset can be applied to the position values of the next directory
        // in the list. TODO: how should this be stored? what kind of offset should be used in order to allow for extra entries to be added to one of the directories
        // while the mount point is open?
        // . and .. directory entries will also need special handling, not sure how that should work either
        return_value(FD_REPLY_ENDPOINT(*message).address, ENOSYS);
        break;
    case FD_STAT:
        mount_point_stat(info, message);
        break;
    case FD_LINK:
        mount_point_link(state->thread_id, info, message);
        break;
    case FD_UNLINK:
        mount_point_unlink(state->thread_id, info, message);
        break;
    case FD_OPEN:
        mount_point_open(state, info, message);
        break;
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_TRUNCATE:
        // writing/truncating is prohibited for directories
        return_value(FD_REPLY_ENDPOINT(*message).address, ENOTSUP);
        break;
    case FD_MOUNT:
        {
            struct ipc_message reply = {.capabilities = {}};
            FD_RETURN_VALUE(reply) = mount(info, FD_MOUNT_FILE_DESCRIPTOR(*message).address, FD_MOUNT_FLAGS(*message));
            syscall_invoke(FD_REPLY_ENDPOINT(*message).address, -1, ENDPOINT_SEND, (size_t) &reply);
        }
        break;
    case FD_UNMOUNT:
        // TODO
        return_value(FD_REPLY_ENDPOINT(*message).address, ENOSYS);
        break;
    default:
        return_value(FD_REPLY_ENDPOINT(*message).address, EBADMSG);
        break;
    }

    syscall_invoke(info_address, -1, UNTYPED_UNLOCK, 0);
}

size_t mount(struct directory_info *info, size_t directory_fd, uint8_t flags) {
    if (info->can_modify_namespace == false) {
        return EPERM;
    }

    size_t namespace_address = (info->namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, -1, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        return ENOMEM;
    }

    // TODO: handle flags properly so existing mount points can be added on to
    // TODO: if directory_fd is a proxied endpoint, copy the original endpoint to prevent any Wackiness
    // TODO: should other mount points be allowed to be mounted like this? it would be nice but it would probably also lead to issues and would require
    // circular reference checking (to prevent multiple vfs worker threads from locking up permanently)

    size_t mounted_list_index = create_mounted_list_entry();

    if (mounted_list_index == -1) {
        syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0);
        return ENOMEM;
    }

    size_t result = mounted_list_insert(mounted_list_index, directory_fd, flags);

    if (result != 0) {
        syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0);
        return result;
    }

    struct mount_point mount_point = {
        .previous = -1,
        .next = -1,
        .references = 1,
        .enclosing_filesystem = info->enclosing_filesystem,
        .inode = info->inode,
        .mounted_list_index = mounted_list_index
    };

    result = add_mount_point_to_namespace(namespace, &mount_point);
    syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0);

    return result;
}
