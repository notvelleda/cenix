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
static void open_file(size_t thread_id, size_t directory_id, size_t endpoint_address, size_t temp_slot, struct ipc_message *message) {
    size_t directory_address = DIRECTORY_ADDRESS(directory_id);
    size_t directory_info_address = DIRECTORY_INFO_ADDRESS(directory_id);
    size_t reply_capability = message->capabilities[0].address;

    struct directory_info *info = (struct directory_info *) syscall_invoke(directory_info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        return return_value(reply_capability, ENOMEM);
    }

    size_t reply_endpoint_address = THREAD_STORAGE_SLOT(thread_id, 7);
    // TODO: send a version of the reply endpoint with access control that only allows for sending messages (should vfs.h be modified for that?),
    // though maybe that could be folded into a potential ENDPOINT_CALL invocation?

    struct open_fields fields = {
        .fd_endpoint = directory_address,
        .reply_endpoint = reply_endpoint_address,
        .name_address = message->capabilities[1].address,
        .flags = message->buffer[1],
        .mode = message->buffer[2]
    };

    // the capability for this gets put in the directories node since if it's a directory it'll stay there, and if it's a regular file it'll
    // be transferred back to the client anyway
    size_t opened_file_address = find_slot_for(USED_DIRECTORY_IDS_SLOT, MAX_OPEN_DIRECTORIES, &fields, &open_file_callback);

    if (opened_file_address == -1) {
        syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
        return return_value(reply_capability, ENOMEM);
    }

    struct stat stat;
    size_t result = fd_stat(opened_file_address, reply_endpoint_address, &stat);

    if (result != 0) {
idk_just_fucking_return:
        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
        syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
        return return_value(reply_capability, result);
    }

    size_t mount_point_address = find_mount_point(info->fs_id, stat.st_ino, info->enclosing_filesystem);

    if (mount_point_address == -1) {
        // TODO: should symlinks be followed here? i suppose it would be easier to not bother with that and handle it in vfs_open and libc

        if (S_ISDIR(stat.st_mode)) {
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

            syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);

            if (result != 0) {
                free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
                syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
            }
        } else {
            struct ipc_message reply = {
                .capabilities = {{opened_file_address, -1}}
            };
            *(size_t *) &reply.buffer = 0;

            syscall_invoke(reply_capability, -1, ENDPOINT_SEND, (size_t) &reply);

            free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
            syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
            return;
        }
    } else {
        // badge the vfs endpoint with the mount point address and send it back to the caller
        result = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(mount_point_address, IPC_FLAG_IS_MOUNT_POINT), reply_capability);

        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
        syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
    }

    if (result != 0) {
        return_value(reply_capability, result);
    }
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
        return open_file(thread_id, directory_id, endpoint_address, temp_slot, message);
    case FD_WRITE:
    case FD_WRITE_FAST:
        // writing is prohibited for directories
        return return_value(message->capabilities[0].address, ENOTSUP);
    default:
        return return_value(message->capabilities[0].address, EBADMSG);
    }
}

void handle_mount_point_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot) {
    size_t mount_point_address = IPC_ID(message->badge); // TODO: should this not just contain the slot number of the structure instead of its full address?

    printf("vfs server: got message %d for mount point %d!\n", message->buffer[0], mount_point_address >> INIT_NODE_DEPTH);

    switch (message->buffer[0]) {
    case FD_READ:
    case FD_READ_FAST:
        // this one is a fucking Mess and idk really how to implement it well. all the calls should be intercepted so that the highest position value can be found
        // for each directory in the mount point, so after the end of a directory is found an offset can be applied to the position values of the next directory
        // in the list. TODO: how should this be stored? what kind of offset should be used in order to allow for extra entries to be added to one of the directories
        // while the mount point is open?
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

            // TODO: stat the directory this mount point is contained within and preserve these values
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
    case FD_LINK:
    case FD_UNLINK:
    case FD_OPEN:
        // these can just be done by trying the call in every directory mounted on this endpoint until it works, and for open if none of them succeed then if
        // creation is requested it can try in the directories marked with MCREATE (or all of them if none are)
        // not sure how link should behave- should it only work for MCREATE flagged directories* or all of them? since it's really only useful for efficient move
        // operations
        // TODO: move open() logic into its own function
    case FD_WRITE:
    case FD_WRITE_FAST:
        // writing is prohibited for directories
        return return_value(message->capabilities[0].address, ENOTSUP);
    default:
        return return_value(message->capabilities[0].address, EBADMSG);
    }
}
