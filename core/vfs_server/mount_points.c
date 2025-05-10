#include "debug.h"
#include "directories.h"
#include "ipc.h"
#include "mount_points.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/vfs.h"
#include "utils.h"

/// \brief helper function to iterate over all filesystems in a mount point and run a callback over each.
///
/// this callback is passed the value of the `data` argument, the address of the root file descriptor of the filesystem,
/// whether this filesystem was flagged with MCREATE, and a pointer to a value that will be returned if the function returns `false`.
/// if it returns `true`, it will continue on to the next filesystem and call the callback again, and so on and so forth
static size_t iterate_over_mount_point(size_t mount_point_address, void *data, bool (*fn)(void *, size_t, bool, size_t *)) {
    // mount point lock is held throughout so that nothing wacky happens. not sure if the performance of this is especially Good but whatever
    struct mount_point *mount_point = (struct mount_point *) syscall_invoke(mount_point_address, -1, UNTYPED_LOCK, 0);

    if (mount_point == NULL) {
        return ENOMEM;
    }

    struct mounted_list_info *list = (struct mounted_list_info *) syscall_invoke(mount_point->first_node, -1, UNTYPED_LOCK, 0);

    if (list == NULL) {
        syscall_invoke(mount_point_address, -1, UNTYPED_UNLOCK, 0);
        return ENOMEM;
    }

    for (int i = 0; i < sizeof(size_t) * 8; i ++) {
        if ((list->used_slots & (1 << i)) == 0) {
            continue;
        }

        bool is_create_flagged = (list->create_flagged_slots & (1 << i)) != 0;
        size_t address = (i << (INIT_NODE_DEPTH + MOUNTED_FS_BITS)) | (mount_point_address & ~((1 << INIT_NODE_DEPTH) - 1)) | MOUNTED_LIST_NODE_SLOT;
        size_t result = 0;

        if (!fn(data, address, is_create_flagged, &result)) {
            syscall_invoke(mount_point->first_node, -1, UNTYPED_UNLOCK, 0);
            syscall_invoke(mount_point_address, -1, UNTYPED_UNLOCK, 0);
            return result;
        }
    }

    syscall_invoke(mount_point->first_node, -1, UNTYPED_UNLOCK, 0);
    syscall_invoke(mount_point_address, -1, UNTYPED_UNLOCK, 0);
    return ENOENT;
}

/// handles FD_STAT calls for mount points
static void mount_point_stat(struct directory_info *info, struct ipc_message *message) {
    struct ipc_message reply = {
        .capabilities = {}
    };
    *(size_t *) &reply.buffer = 0;

    struct stat *stat = (struct stat *) &reply.buffer[sizeof(size_t)];
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

    syscall_invoke(message->capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &reply);
}

struct link_args {
    struct ipc_message to_send;
    bool requires_create_flag;
};

/// iterate_over_mount_point() callback to handle FD_LINK/FD_UNLINK for mount points
static bool link_callback(void *data, size_t directory_address, bool is_create_flagged, size_t *result_value) {
    struct link_args *args = (struct link_args *) data;

    if (!is_create_flagged && args->requires_create_flag) {
        // FD_LINK in a union directory should only successfully link in directories marked with MCREATE
        return true;
    }

    struct ipc_message to_receive = {
        .capabilities = {}
    };

    // TODO: have a timeout on this
    size_t result = vfs_call(directory_address, args->to_send.capabilities[0].address, &args->to_send, &to_receive);

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
            .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, message->capabilities[1], message->capabilities[2]},
            .to_copy = 7
        },
        .requires_create_flag = true
    };
    return_value(message->capabilities[0].address, iterate_over_mount_point(info->mount_point_address, &args, &link_callback));
}

/// handles FD_UNLINK calls in mount points
static void mount_point_unlink(size_t thread_id, struct directory_info *info, struct ipc_message *message) {
    struct link_args args = {
        .to_send = {
            .buffer = {FD_UNLINK},
            .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, message->capabilities[1]},
            .to_copy = 3
        },
        .requires_create_flag = false
    };
    return_value(message->capabilities[0].address, iterate_over_mount_point(info->mount_point_address, &args, &link_callback));
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
        return_value(message->capabilities[0].address, ENOMEM);
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

    if ((message->buffer[1] & (OPEN_CREATE | OPEN_EXCLUSIVE)) == (OPEN_CREATE | OPEN_EXCLUSIVE)) {
        // TODO: fail if a file with this name already exists
        args.allow_create = true;
        result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);
    } else {
        bool should_create = (message->buffer[1] & OPEN_CREATE) != 0;
        message->buffer[1] &= ~OPEN_CREATE;

        syscall_invoke(info->mount_point_address, -1, UNTYPED_UNLOCK, 0);

        result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);

        if (result != 0 && should_create) {
            args.allow_create = true;
            message->buffer[1] |= OPEN_CREATE;
            result = iterate_over_mount_point(info->mount_point_address, &args, &open_callback);
        }
    }

    if (result != 0) {
        return_value(message->capabilities[0].address, result);
    }
}

void handle_mount_point_message(const struct state *state, struct ipc_message *message) {
    size_t info_address = IPC_ID(message->badge); // TODO: should this not just contain the slot number of the structure instead of its full address?

    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        return_value(message->capabilities[0].address, ENOMEM);
        return;
    }

    printf("vfs_server: got message %d for mount point %d!\n", message->buffer[0], info->mount_point_address >> INIT_NODE_DEPTH);

    switch (message->buffer[0]) {
    case FD_READ:
    case FD_READ_FAST:
        // TODO: this one is a fucking Mess and idk really how to implement it well. all the calls should be intercepted so that the highest position value can be found
        // for each directory in the mount point, so after the end of a directory is found an offset can be applied to the position values of the next directory
        // in the list. TODO: how should this be stored? what kind of offset should be used in order to allow for extra entries to be added to one of the directories
        // while the mount point is open?
        // . and .. directory entries will also need special handling, not sure how that should work either
        return_value(message->capabilities[0].address, ENOSYS);
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
        return_value(message->capabilities[0].address, ENOTSUP);
        break;
    default:
        return_value(message->capabilities[0].address, EBADMSG);
        break;
    }

    syscall_invoke(info_address, -1, UNTYPED_UNLOCK, 0);
}
