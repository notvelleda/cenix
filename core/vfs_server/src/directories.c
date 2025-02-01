#include "debug.h"
#include "directories.h"
#include "ipc.h"
#include "namespaces.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/vfs.h"

struct open_fields {
    size_t fd_endpoint;
    size_t reply_endpoint;
    size_t name_address;
    uint8_t flags;
    uint8_t mode;
};

static size_t open_file_callback(void *data, size_t slot) {
    struct open_fields *fields = (struct open_fields *) data;
    size_t fd_address = (slot << INIT_NODE_DEPTH) | DIRECTORY_NODE_SLOT;

    size_t result = fd_open(fields->fd_endpoint, fields->reply_endpoint, fields->name_address, fd_address, fields->flags, fields->mode);

    if (result == 0) {
        return fd_address;
    } else {
        return -1;
    }
}

/// badges the vfs endpoint with the given badge and sends the new endpoint back to the caller process
static size_t badge_and_send(size_t thread_id, size_t vfs_endpoint_address, size_t temp_slot, size_t badge, size_t reply_endpoint_address) {
    const struct node_copy_args copy_args = {
        .source_address = vfs_endpoint_address,
        .source_depth = -1,
        .dest_slot = temp_slot,
        .access_rights = -1, // TODO: set access rights so that other processes can't listen on this endpoint
        .badge = badge,
        .should_set_badge = 1
    };
    size_t result = syscall_invoke(THREAD_STORAGE_ADDRESS(thread_id), THREAD_STORAGE_DEPTH, NODE_COPY, (size_t) &copy_args);

    if (result != 0) {
        return result;
    }

    struct ipc_message reply = {
        .capabilities = {{THREAD_STORAGE_SLOT(thread_id, temp_slot), THREAD_STORAGE_SLOT_DEPTH}}
    };
    *(size_t *) &reply.buffer = 0; // TODO: is this necessary? will the compiler zero out the buffer anyway?

    syscall_invoke(reply_endpoint_address, -1, ENDPOINT_SEND, (size_t) &reply);

    // copied capability is deleted just in case
    syscall_invoke(THREAD_STORAGE_ADDRESS(thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, temp_slot);

    return 0;
}

static void return_value(size_t reply_capability, size_t error_code) {
    struct ipc_message reply = {
        .capabilities = {}
    };
    *(size_t *) &reply.buffer = error_code;

    syscall_invoke(reply_capability, -1, ENDPOINT_SEND, (size_t) &reply);
}

struct directory_info {
    size_t fs_id;
    size_t enclosing_filesystem;
};

#define DIRECTORY_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_NODE_SLOT)
#define DIRECTORY_INFO_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT)

/// opens the requested file, then calls fd_stat on it to check if its inode matches that of a mount point. if it doesn't, the newly opened file
/// is sent back to the calling process. if it does match, a new directory endpoint is created for the root directory of the new filesystem
static size_t open_file(size_t thread_id, size_t fd_endpoint, size_t endpoint_address, size_t temp_slot, struct directory_info *info, struct ipc_message *message) {
    size_t reply_capability = message->capabilities[0].address;

    // TODO: have a special case for .. both in mount points and in directories 1 level above mount points to hopefully prevent any wackiness there

    size_t reply_endpoint_address = THREAD_STORAGE_SLOT(thread_id, REPLY_ENDPOINT_SLOT);
    // TODO: send a version of the reply endpoint with access control that only allows for sending messages (should vfs.h be modified for that?),
    // though maybe that could be folded into a potential ENDPOINT_CALL invocation?

    struct open_fields fields = {
        .fd_endpoint = fd_endpoint,
        .reply_endpoint = reply_endpoint_address,
        .name_address = message->capabilities[1].address,
        .flags = message->buffer[1],
        .mode = message->buffer[2]
    };

    // the capability for this gets put in the directories node since if it's a directory it'll stay there, and if it's a regular file it'll
    // be transferred back to the client anyway
    size_t opened_file_address = find_slot_for(USED_DIRECTORY_IDS_SLOT, MAX_OPEN_DIRECTORIES, &fields, &open_file_callback);

    if (opened_file_address == -1) {
        return ENOMEM;
    }

    struct stat stat;
    size_t result = fd_stat(opened_file_address, reply_endpoint_address, &stat);

    if (result != 0) {
idk_just_fucking_return:
        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
        return result;
    }

    size_t mount_point_address = find_mount_point(info->fs_id, stat.st_ino, info->enclosing_filesystem);

    if (mount_point_address != -1) {
        // badge the vfs endpoint with the mount point address and send it back to the caller
        result = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(mount_point_address, IPC_FLAG_IS_MOUNT_POINT), reply_capability);
        goto idk_just_fucking_return;
    }

    if (!S_ISDIR(stat.st_mode)) {
        // if this isn't a directory then just reply with the newly opened capability, as directories are the only things proxied

        struct ipc_message reply = {
            .capabilities = {{opened_file_address, -1}}
        };
        *(size_t *) &reply.buffer = 0;

        syscall_invoke(reply_capability, -1, ENDPOINT_SEND, (size_t) &reply);

        //result = 0;
        goto idk_just_fucking_return; // result should be 0 here since it's not modified since the last time it's checked
    }

    // since this is a directory and it's not a mount point, a new directory_info struct is created to go along with the directory endpoint
    // before badging the directory id and sending an endpoint with it back to the caller

    size_t new_directory_id = opened_file_address >> INIT_NODE_DEPTH;

    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = sizeof(struct directory_info),
        .address = (new_directory_id << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT,
        .depth = -1
    };

    result = syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    if (result != 0) {
        goto idk_just_fucking_return; // i am So Tired of not having destructors (or even just defer). i long for the crab
    }

    struct directory_info *new_directory_info = (struct directory_info *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);

    if (new_directory_info == NULL) {
        syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
        goto idk_just_fucking_return;
    }

    *new_directory_info = *info; // this will be the same as the parent directory's directory_info since they're both on the same filesystem

    syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

    result = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(new_directory_id, IPC_FLAG_IS_MOUNT_POINT), reply_capability);

    if (result != 0) {
        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
        syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
    }

    return result;
}

void handle_directory_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot) {
    size_t directory_id = IPC_ID(message->badge);

    printf("vfs server: got message %d for directory %d!\n", message->buffer[0], directory_id);

    // TODO: should there be a check for if this directory is now a mount point? does the potential benefit outweigh the performance impact?

    switch (message->buffer[0]) {
    case FD_READ:
    case FD_READ_FAST:
    case FD_STAT:
    case FD_LINK:
    case FD_UNLINK:
        // pass this directly thru to the endpoint at directory_address, where the reply endpoint it's given is the one passed to this message
        // so that fewer syscalls occur and so the vfs server itself doesn't have to deal with it
        // TODO: should FD_UNLINK be checked in case it refers to a mount point?

        {
            uint8_t transferred_capabilities = message->transferred_capabilities;
            message->to_copy = 0; // make sure all capabilities are moved

            syscall_invoke(DIRECTORY_ADDRESS(directory_id), -1, ENDPOINT_SEND, (size_t) message);

            message->transferred_capabilities ^= transferred_capabilities; // make sure any capabilities that aren't transferred are cleaned up in _start()
        }

        break;
    case FD_OPEN:
        {
            size_t directory_address = DIRECTORY_ADDRESS(directory_id);
            size_t directory_info_address = DIRECTORY_INFO_ADDRESS(directory_id);

            struct directory_info *info = (struct directory_info *) syscall_invoke(directory_info_address, -1, UNTYPED_LOCK, 0);

            if (info == NULL) {
                return return_value(message->capabilities[0].address, ENOMEM);
            }

            size_t result = open_file(thread_id, directory_address, endpoint_address, temp_slot, info, message);

            syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);

            if (result != 0) {
                return_value(message->capabilities[0].address, result);
            }
        }

        break;
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_TRUNCATE:
        // writing/truncating is prohibited for directories
        return return_value(message->capabilities[0].address, ENOTSUP);
    default:
        return return_value(message->capabilities[0].address, EBADMSG);
    }
}

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

struct link_args {
    struct ipc_message to_send;
    bool requires_create_flag;
};

/// handles FD_LINK/FD_UNLINK for mount points
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

struct open_args {
    size_t thread_id;
    size_t endpoint_address;
    size_t temp_slot;
    struct directory_info info;
    struct ipc_message *message;
    bool allow_create;
};

static bool open_callback(void *data, size_t directory_address, bool is_create_flagged, size_t *result_value) {
    struct open_args *args = (struct open_args *) data;

    if (is_create_flagged && !args->allow_create) {
        return true;
    }

    size_t result = open_file(args->thread_id, directory_address, args->endpoint_address, args->temp_slot, &args->info, args->message);

    if (result == 0) {
        *result_value = 0;
        return false;
    } else {
        return true;
    }
}

void handle_mount_point_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot) {
    size_t mount_point_address = IPC_ID(message->badge); // TODO: should this not just contain the slot number of the structure instead of its full address?

    printf("vfs server: got message %d for mount point %d!\n", message->buffer[0], mount_point_address >> INIT_NODE_DEPTH);

    switch (message->buffer[0]) {
    case FD_READ:
    case FD_READ_FAST:
        // TODO: this one is a fucking Mess and idk really how to implement it well. all the calls should be intercepted so that the highest position value can be found
        // for each directory in the mount point, so after the end of a directory is found an offset can be applied to the position values of the next directory
        // in the list. TODO: how should this be stored? what kind of offset should be used in order to allow for extra entries to be added to one of the directories
        // while the mount point is open?
        // . and .. directory entries will also need special handling, not sure how that should work either
        return return_value(message->capabilities[0].address, ENOSYS);
    case FD_STAT:
        {
            struct ipc_message reply = {
                .capabilities = {}
            };
            *(size_t *) &reply.buffer = 0;

            struct stat *stat = (struct stat *) &reply.buffer[sizeof(size_t)];
            stat->st_dev = mount_point_address;
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

        break;
    case FD_LINK:
        {
            struct link_args args = {
                .to_send = {
                    .buffer = {FD_LINK},
                    .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, message->capabilities[1], message->capabilities[2]},
                    .to_copy = 7
                },
                .requires_create_flag = true
            };
            return_value(message->capabilities[0].address, iterate_over_mount_point(mount_point_address, &args, &link_callback));
        }

        break;
    case FD_UNLINK:
        {
            struct link_args args = {
                .to_send = {
                    .buffer = {FD_UNLINK},
                    .capabilities = {{THREAD_STORAGE_SLOT(thread_id, 7), -1}, message->capabilities[1]},
                    .to_copy = 3
                },
                .requires_create_flag = false
            };
            return_value(message->capabilities[0].address, iterate_over_mount_point(mount_point_address, &args, &link_callback));
        }

        break;
    case FD_OPEN:
        {
            struct mount_point *mount_point = (struct mount_point *) syscall_invoke(mount_point_address, -1, UNTYPED_LOCK, 0);

            if (mount_point == NULL) {
                return_value(message->capabilities[0].address, ENOMEM);
            }

            struct open_args args = {
                .thread_id = thread_id,
                .endpoint_address = endpoint_address,
                .temp_slot = temp_slot,
                .info = {
                    .fs_id = 0, // TODO: figure this out somehow lmao
                    .enclosing_filesystem = mount_point->enclosing_filesystem
                },
                .message = message,
                .allow_create = false
            };
            size_t result;

            if ((message->buffer[1] & (OPEN_CREATE | OPEN_EXCLUSIVE)) == (OPEN_CREATE | OPEN_EXCLUSIVE)) {
                // TODO: fail if a file with this name already exists
                args.allow_create = true;
                result = iterate_over_mount_point(mount_point_address, &args, &open_callback);
            } else {
                bool should_create = (message->buffer[1] & OPEN_CREATE) != 0;
                message->buffer[1] &= ~OPEN_CREATE;

                syscall_invoke(mount_point_address, -1, UNTYPED_UNLOCK, 0);

                result = iterate_over_mount_point(mount_point_address, &args, &open_callback);

                if (result != 0 && should_create) {
                    args.allow_create = true;
                    message->buffer[1] |= OPEN_CREATE;
                    result = iterate_over_mount_point(mount_point_address, &args, &open_callback);
                }
            }

            if (result != 0) {
                return_value(message->capabilities[0].address, result);
            }
        }

        break;
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_TRUNCATE:
        // writing/truncating is prohibited for directories
        return return_value(message->capabilities[0].address, ENOTSUP);
    default:
        return return_value(message->capabilities[0].address, EBADMSG);
    }
}

size_t open_root(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot, size_t fs_id) {
    size_t reply_capability = message->capabilities[0].address;

    // get the namespace from the fs id
    size_t namespace_address = (fs_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, -1, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        return ENOMEM;
    }

    // badge the vfs endpoint with the root mount point address for this namespace and send it back to the caller
    size_t result = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(namespace->root_address, IPC_FLAG_IS_MOUNT_POINT), reply_capability);

    // TODO: there needs to be some way to save fs_id here since it'll be lost otherwise, and that info is needed for fd_open to work

    syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0); // lock is held throughout, not sure if this is the right call (a race condition here would be fine tbh) but it is what it is

    return result;
}
