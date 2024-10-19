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

struct directory_info {
    size_t fs_id;
    size_t enclosing_filesystem;
};

void handle_directory_message(size_t thread_id, struct ipc_message *message, size_t endpoint_address, size_t temp_slot) {
    size_t directory_id = IPC_ID(message->badge);

    printf("vfs server: got message %d for directory %d!\n", message->buffer[0], directory_id);

    size_t directory_address = (directory_id << INIT_NODE_DEPTH) | DIRECTORY_NODE_SLOT;
    size_t directory_info_address = (directory_id << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT;
    size_t return_value = 1;

    // TODO: should there be a check for if this directory is now a mount point? does the potential benefit outweigh the performance impact?

    switch (message->buffer[0]) {
    case FD_READ:
    case FD_READ_FAST:
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_STAT:
    case FD_LINK:
    case FD_UNLINK:
        // pass this directly thru to the endpoint at directory_address, where the reply endpoint it's given is the one passed to this message
        // so that fewer syscalls occur and so the vfs server itself doesn't have to deal with it
        // TODO: should FD_UNLINK be checked in case it refers to a mount point?

        uint8_t transferred_capabilities = message->transferred_capabilities;
        message->to_copy = 0; // make sure all capabilities are moved

        syscall_invoke(directory_address, -1, ENDPOINT_SEND, (size_t) message);

        message->transferred_capabilities ^= transferred_capabilities; // make sure any capabilities that aren't transferred are cleaned up in _start()

        break;
    case FD_OPEN:
        // open the requested file, then call fd_stat on it to check if its inode matches that of a mount point. if it doesn't, the newly opened file
        // can be sent back to the calling process. if it does match, a new directory endpoint can be created for the root directory of the new filesystem

        struct directory_info *info = (struct directory_info *) syscall_invoke(directory_info_address, -1, UNTYPED_LOCK, 0);

        if (info == NULL) {
            goto return_value;
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
            goto return_value;
        }

        struct stat stat;
        return_value = fd_stat(opened_file_address, reply_endpoint_address, &stat);

        if (return_value != 0) {
idk_just_fucking_return:
            free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
            syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
            goto return_value;
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

                return_value = syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

                if (return_value != 0) {
                    goto idk_just_fucking_return; // i am So Tired of not having destructors (or even just defer). i long for the crab
                }

                struct directory_info *new_directory_info = (struct directory_info *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);

                if (new_directory_info == NULL) {
                    syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
                    goto idk_just_fucking_return;
                }

                *new_directory_info = *info; // this will be the same as the parent directory's directory_info since they're both on the same filesystem

                syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

                return_value = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(new_directory_id, IPC_FLAG_IS_MOUNT_POINT), message->capabilities[0].address);

                syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);

                if (return_value != 0) {
                    free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
                    syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
                    goto return_value;
                } else {
                    return;
                }
            } else {
                struct ipc_message reply = {
                    .capabilities = {{opened_file_address, -1}}
                };
                *(size_t *) &reply.buffer = 0;

                syscall_invoke(message->capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &reply);

                free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
                syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
                return;
            }
        } else {
            // badge the vfs endpoint with the mount point address and send it back to the caller
            return_value = badge_and_send(thread_id, endpoint_address, temp_slot, IPC_BADGE(mount_point_address, IPC_FLAG_IS_MOUNT_POINT), message->capabilities[0].address);

            free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
            syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);

            if (return_value != 0) {
                goto return_value;
            } else {
                return;
            }
        }

        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_NODE_SLOT, MAX_OPEN_DIRECTORIES, opened_file_address);
        syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);
        return_value = 0;
        // fall thru to return_value
    default:
        // return_value is 1 by default, so an error will be returned if the command isn't recognized
return_value:
        struct ipc_message reply = {
            .capabilities = {}
        };
        *(size_t *) &reply.buffer = return_value;

        syscall_invoke(message->capabilities[0].address, -1, ENDPOINT_SEND, (size_t) &reply);
        break;
    }
}
