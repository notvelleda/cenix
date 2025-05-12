#include "debug.h"
#include "directories.h"
#include "ipc.h"
#include "namespaces.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/vfs.h"
#include "utils.h"

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

#define DIRECTORY_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_NODE_SLOT)
#define DIRECTORY_INFO_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT)

static size_t open_mount_point(const struct state *state, size_t opened_file_address, size_t mount_point_address, struct directory_info *info, ino_t inode, size_t reply_address) {
    size_t opened_file_slot = opened_file_address >> INIT_NODE_DEPTH;
    size_t info_address = DIRECTORY_INFO_ADDRESS(opened_file_slot);

    // allocate memory for the directory info structure
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = sizeof(struct directory_info),
        .address = info_address,
        .depth = -1
    };
    size_t result = syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    if (result != 0) {
        return result;
    }

    // delete this capability since it's not needed
    syscall_invoke(DIRECTORY_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, opened_file_slot);

    struct directory_info *new_info = (struct directory_info *) syscall_invoke(info_address, -1, UNTYPED_LOCK, 0);

    if (new_info == NULL) {
        return ENOMEM;
    }

    new_info->namespace_id = info->namespace_id;
    new_info->can_modify_namespace = info->can_modify_namespace;
    new_info->inode = inode;
    new_info->mount_point_address = mount_point_address;

    syscall_invoke(info_address, -1, UNTYPED_UNLOCK, 0);

    // badge the vfs endpoint with the address of the directory info structure and send it back to the caller
    return badge_and_send(state, IPC_BADGE(info_address, IPC_FLAG_IS_MOUNT_POINT), reply_address);
}

static size_t proxy_directory(const struct state *state, size_t opened_file_address, struct directory_info *info, ino_t inode, size_t reply_address) {
    // since this is a directory and it's not a mount point, a new directory_info struct is created to go along with the directory endpoint
    // before badging the directory id and sending an endpoint with it back to the caller

    size_t new_directory_id = opened_file_address >> INIT_NODE_DEPTH;

    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = sizeof(struct directory_info),
        .address = (new_directory_id << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT,
        .depth = -1
    };

    size_t result = syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    if (result != 0) {
        return result;
    }

    struct directory_info *new_directory_info = (struct directory_info *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);

    if (new_directory_info == NULL) {
        syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
        return result;
    }

    *new_directory_info = *info; // this will be mostly the same as the parent directory's directory_info since they're both on the same filesystem
    new_directory_info->inode = inode;

    syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

    result = badge_and_send(state, IPC_BADGE(new_directory_id, IPC_FLAG_IS_MOUNT_POINT), reply_address);

    if (result != 0) {
        syscall_invoke(DIRECTORY_INFO_SLOT, INIT_NODE_DEPTH, NODE_DELETE, new_directory_id);
    }

    return result;
}

size_t open_file(const struct state *state, size_t fd_endpoint, struct directory_info *info, struct ipc_message *message) {
    size_t reply_capability = FD_REPLY_ENDPOINT(*message).address;

    // TODO: have a special case for .. both in mount points and in directories 1 level above mount points to hopefully prevent any wackiness there

    size_t reply_endpoint_address = THREAD_STORAGE_SLOT(state->thread_id, REPLY_ENDPOINT_SLOT);
    // TODO: send a version of the reply endpoint with access control that only allows for sending messages (should vfs.h be modified for that?),
    // though maybe that could be folded into a potential ENDPOINT_CALL invocation?

    struct open_fields fields = {
        .fd_endpoint = fd_endpoint,
        .reply_endpoint = reply_endpoint_address,
        .name_address = FD_OPEN_NAME_ADDRESS(*message).address,
        .flags = FD_OPEN_FLAGS(*message),
        .mode = FD_OPEN_MODE(*message)
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

    size_t mount_point_address = find_mount_point(info->namespace_id, stat.st_ino, info->enclosing_filesystem);

    if (mount_point_address != -1) {
        // this directory is a mount point, so it needs to be handled accordingly

        size_t result = open_mount_point(state, opened_file_address, mount_point_address, info, stat.st_ino, reply_capability);

        if (result != 0) {
            goto idk_just_fucking_return; // i am So Tired of not having destructors (or even just defer). i long for the crab
        }

        return result;
    } else if (!S_ISDIR(stat.st_mode)) {
        // this isn't a directory, so just reply with the newly opened capability as directories are the only things proxied

        struct ipc_message reply = {
            .capabilities = {{opened_file_address, -1}}
        };
        FD_RETURN_VALUE(reply) = 0;

        syscall_invoke(reply_capability, -1, ENDPOINT_SEND, (size_t) &reply);

        //result = 0;
        goto idk_just_fucking_return; // result should be 0 here since it's not modified since the last time it's checked
    } else {
        // proxy the directory and send that back to the caller

        size_t result = proxy_directory(state, opened_file_address, info, stat.st_ino, reply_capability);

        if (result != 0) {
            goto idk_just_fucking_return;
        }

        return result;
    }
}

/// passes a directory message through to the filesystem server, which will then send the response from that back to the calling process
static void pass_thru_directory_message(size_t directory_id, struct ipc_message *message) {
    // pass this directly thru to the endpoint at directory_address, where the reply endpoint it's given is the one passed to this message
    // so that fewer syscalls occur and so the vfs server itself doesn't have to deal with it
    // TODO: should FD_UNLINK be checked in case it refers to a mount point?

    uint8_t transferred_capabilities = message->transferred_capabilities;
    message->to_copy = 0; // make sure all capabilities are moved

    syscall_invoke(DIRECTORY_ADDRESS(directory_id), -1, ENDPOINT_SEND, (size_t) message);

    message->transferred_capabilities ^= transferred_capabilities; // make sure any capabilities that aren't transferred are cleaned up in _start()
}

/// handles FD_OPEN calls for directories
static void directory_open(const struct state *state, size_t directory_id, struct ipc_message *message) {
    size_t directory_address = DIRECTORY_ADDRESS(directory_id);
    size_t directory_info_address = DIRECTORY_INFO_ADDRESS(directory_id);

    struct directory_info *info = (struct directory_info *) syscall_invoke(directory_info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        return return_value(FD_REPLY_ENDPOINT(*message).address, ENOMEM);
    }

    size_t result = open_file(state, directory_address, info, message);

    syscall_invoke(directory_info_address, -1, UNTYPED_UNLOCK, 0);

    if (result != 0) {
        return_value(FD_REPLY_ENDPOINT(*message).address, result);
    }
}

void handle_directory_message(const struct state *state, struct ipc_message *message) {
    size_t directory_id = IPC_ID(message->badge);

    printf("vfs_server: got message %d for directory %d!\n", FD_CALL_NUMBER(*message), directory_id);

    // TODO: should there be a check for if this directory is now a mount point? does the potential benefit outweigh the performance impact?

    switch (FD_CALL_NUMBER(*message)) {
    case FD_READ:
    case FD_READ_FAST:
    case FD_STAT:
    case FD_LINK:
    case FD_UNLINK:
        pass_thru_directory_message(directory_id, message);
        break;
    case FD_OPEN:
        directory_open(state, directory_id, message);
        break;
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_TRUNCATE:
        // writing/truncating is prohibited for directories
        return return_value(FD_REPLY_ENDPOINT(*message).address, ENOTSUP);
    case FD_MOUNT:
        {
            struct directory_info *info = (struct directory_info *) syscall_invoke(directory_id, -1, UNTYPED_LOCK, 0);

            if (info == NULL) {
                return_value(FD_REPLY_ENDPOINT(*message).address, ENOMEM);
                return;
            }

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
        return return_value(FD_REPLY_ENDPOINT(*message).address, EBADMSG);
    }
}

size_t open_root(const struct state *state, struct ipc_message *message, size_t namespace_id) {
    size_t reply_capability = FD_REPLY_ENDPOINT(*message).address;

    // get the namespace object from the namespace id
    size_t namespace_address = (namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
    struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, -1, UNTYPED_LOCK, 0);

    if (namespace == NULL) {
        return ENOMEM;
    }

    // allocate a structure to store the namespace id and root mount point address
    size_t info_address = alloc_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_INFO_SLOT, MAX_OPEN_DIRECTORIES, sizeof(struct directory_info));

    if (info_address == -1) {
        return ENOMEM;
    }

    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, -1, UNTYPED_LOCK, 0);

    if (info == NULL) {
        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_INFO_SLOT, MAX_OPEN_DIRECTORIES, info_address);
        return ENOMEM;
    }

    info->namespace_id = namespace_id;
    info->can_modify_namespace = IPC_FLAGS(message->badge) == IPC_FLAG_CAN_MODIFY;
    info->inode = 0;
    info->mount_point_address = namespace->root_address;

    syscall_invoke(info_address, -1, UNTYPED_UNLOCK, 0);

    // badge the vfs endpoint with the address of the directory info structure and send it back to the caller
    size_t result = badge_and_send(state, IPC_BADGE(info_address, IPC_FLAG_IS_MOUNT_POINT), reply_capability);

    if (result != 0) {
        free_structure(USED_DIRECTORY_IDS_SLOT, DIRECTORY_INFO_SLOT, MAX_OPEN_DIRECTORIES, info_address);
        return result;
    }

    syscall_invoke(namespace_address, -1, UNTYPED_UNLOCK, 0); // lock is held throughout, not sure if this is the right call (a race condition here would be fine tbh) but it is what it is

    return result;
}
