#include "assert.h"
#include "core_io.h"
#include "jax.h"
#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "sys/stat.h"
#include "sys/vfs.h"
#include "test_macros.h"

#define VFS_ENDPOINT_ADDRESS 2

static const void *next_file(const void *address, const void *initrd_end, struct jax_file *file) {
    struct jax_iterator iterator = {
        .start = (const uint8_t *) address,
        .end = (const uint8_t *) initrd_end
    };

    return jax_next_file(&iterator, file) ? iterator.start : NULL;
}

/// \brief gets the next file after the file pointed to by `address` in the given directory.
///
/// the `dir_file` argument is a pointer to the `jax_file` struct for the directory being read, the file's details are placed in the `jax_file` struct pointed to by `file`,
/// and a pointer to the raw data of the file immediately following and its header is returned if there are more files in the archive
/// (which can be fed back into this function as the `address` argument to list the next file in the directory, and so on and so forth)
static const void *next_file_in_dir(struct jax_file *dir_file, const void *address, const void *initrd_start, const void *initrd_end, struct jax_file *file, const void **file_address) {
    // sanity check
    if (address < initrd_start) {
        return NULL;
    }

    const void *previous_address;

    while (previous_address = address, (address = next_file(address, initrd_end, file)) != NULL) {
        int i;

        if (dir_file->name_length == 0) {
            // handle the root directory case, just a path separator check from the start of the name string since files in the root directory don't have /s at the start of their paths

            for (i = 0; i < file->name_length && file->name[i] != '/'; i ++);

            if (i < file->name_length && file->name[i] == '/') {
                continue;
            }
        } else {
            // make sure the filename is long enough to fit the prefix
            if (file->name_length < dir_file->name_length + 1) {
                continue;
            }

            // make sure the prefix is present
            for (i = 0; i < dir_file->name_length && dir_file->name[i] == file->name[i]; i ++);

            if ((i < dir_file->name_length && dir_file->name[i] != file->name[i]) || file->name[dir_file->name_length] != '/') {
                continue;
            }

            // make sure there aren't any more path separators
            for (i = dir_file->name_length + 1; i < file->name_length && file->name[i] != '/'; i ++);

            if (i < file->name_length && file->name[i] == '/') {
                continue;
            }
        }

        // return the address of this file
        if (file_address != NULL) {
            *file_address = previous_address;
        }

        // return the address of the next file
        return address;
    }

    return NULL;
}

/// \brief handles a fd_read/fd_read_fast call.
///
/// 
static size_t handle_read(const struct state *state, struct ipc_message *received, struct ipc_message *reply, struct jax_file *file, size_t position, size_t read_size, void *read_data) {
    if (file->type == TYPE_DIRECTORY) {
        // read a directory entry from this directory

        if (read_size < sizeof(struct vfs_directory_entry)) {
            read_size = sizeof(struct vfs_directory_entry); // this is just advisory anyway it doesn't matter
        }

        const void *current_file_pointer, *temp_file_pointer;
        struct vfs_directory_entry *entry = (struct vfs_directory_entry *) read_data;
        struct jax_file current_file, temp_file;

        // if this is the first file in the directory, use the address of the directory itself so it'll find the first subsequent directory entry if there is any
        const void *read_address = position == 0 ? (const void *) received->badge : (const void *) position;

        // make sure the read address isn't before the start of the initrd, the check for it being after the end is done in jax_next_file() and handled below
        if (read_address < (void *) state->iterator.start && read_address != 0) {
            FD_RETURN_VALUE(*reply) = EINVAL;
            return 0;
        }

        const void *next = next_file_in_dir(file, read_address, (const void *) state->initrd_start, (const void *) state->initrd_end, &current_file, &current_file_pointer);

        if (next == NULL) {
            // this file isn't valid (likely is just garbage data at the end of the initrd)
            FD_RETURN_VALUE(*reply) = EINVAL;
            return 0;
        }

        // TODO: make this emit . and .. directory entries, how would that work here?

        // this is done to make sure there'll be a next file after this directory entry
        if (next_file_in_dir(file, next, (const void *) state->initrd_start, (const void *) state->initrd_end, &temp_file, &temp_file_pointer) == NULL) {
            entry->next_entry_position = 0;
        } else {
            entry->next_entry_position = (size_t) next;
        }

        entry->inode = (ino_t) current_file_pointer;

        size_t name_length = (uint16_t) (current_file.name_length - (file->name_length == 0 ? 0 : file->name_length - 1));
        entry->name_length = (read_size - sizeof(struct vfs_directory_entry)) < name_length ? read_size - sizeof(struct vfs_directory_entry) : name_length;

        read_size = entry->name_length + sizeof(struct vfs_directory_entry);

        memcpy(entry->name, current_file.name + (file->name_length == 0 ? 0 : file->name_length + 1), entry->name_length);
    } else {
        // read from the file

        if (position >= (size_t) file->size) { // this all assumes the file size is positive, but it really shouldn't be negative so it's Probably Fine™
            read_size = 0; // this case is to avoid overflow :3
        } else if (position + read_size >= (size_t) file->size) {
            read_size = (size_t) file->size - position;
        }

        memcpy(read_data, file->data + position, read_size);
    }

    FD_RETURN_VALUE(*reply) = 0;
    return read_size;
}

/// \brief handles FD_OPEN calls.
///
/// the arguments passed to this function are the same as what's passed to `handle_fd_message()`
static void handle_open(const struct state *state, struct ipc_message *received, struct ipc_message *reply, struct jax_file *file, const void *next) {
    if (file->type != TYPE_DIRECTORY) {
        FD_RETURN_VALUE(*reply) = ENOTSUP;
        return;
    }

    if ((FD_OPEN_FLAGS(*received) & OPEN_EXCLUSIVE) != 0 || (FD_OPEN_MODE(*received) & (MODE_WRITE | MODE_APPEND)) != 0) {
        FD_RETURN_VALUE(*reply) = EROFS;
        return;
    }

    const void *current_file_pointer;
    struct jax_file current_file;

    const uint8_t *filename = (const uint8_t *) syscall_invoke(FD_OPEN_NAME_ADDRESS(*received).address, FD_OPEN_NAME_ADDRESS(*received).depth, UNTYPED_LOCK, 0);

    if (filename == NULL) {
        FD_RETURN_VALUE(*reply) = ENOMEM;
        return;
    }

    // find a file in this directory that matches the given filename
    while ((next = next_file_in_dir(file, next, (const void *) state->initrd_start, (const void *) state->initrd_end, &current_file, &current_file_pointer)) != NULL) {
        int i, j;

        for ((i = file->name_length == 0 ? 0 : (file->name_length + 1)), j = 0; i < current_file.name_length && current_file.name[i] == filename[j]; i ++, j ++);

        if (current_file.name[i] != filename[j]) {
            continue;
        }

        // found a matching file!

        // badge the endpoint with the raw address of the file
        const struct node_copy_args copy_args = {
            .source_address = state->endpoint.address,
            .source_depth = state->endpoint.depth,
            .dest_slot = IPC_CAPABILITY_SLOTS + 1,
            .access_rights = UINT8_MAX, // TODO: set access rights so that other processes can't listen on this endpoint
            .badge = (size_t) current_file_pointer,
            .should_set_badge = 1
        };
        size_t result = syscall_invoke(state->node.address, state->node.depth, NODE_COPY, (size_t) &copy_args);

        FD_RETURN_VALUE(*reply) = result;

        if (result == 0) {
            // send the newly badged endpoint back to the caller
            FD_OPEN_REPLY_FD(*reply).address = ((IPC_CAPABILITY_SLOTS + 1) << INIT_NODE_DEPTH) | state->node.address;
            FD_OPEN_REPLY_FD(*reply).depth = SIZE_MAX;
            FD_RETURN_VALUE(*reply) = 0;
        }

        syscall_invoke(FD_OPEN_NAME_ADDRESS(*received).address, FD_OPEN_NAME_ADDRESS(*received).depth, UNTYPED_UNLOCK, 0);
        return;
    }

    // no file was found :(
    FD_RETURN_VALUE(*reply) = (FD_OPEN_FLAGS(*received) & OPEN_CREATE) != 0 ? EROFS : ENOENT;
    syscall_invoke(FD_OPEN_NAME_ADDRESS(*received).address, FD_OPEN_NAME_ADDRESS(*received).depth, UNTYPED_UNLOCK, 0);
}

/// \brief handles an incoming ipc message once the file/directory it should operate on has been found.
///
/// the `state`, `received`, and `reply` arguments are the same as what's passed to `handle_ipc_message()`, the `file` argument points to the file this function should operate on,
/// and the `next` argument points to the raw data of the file (and its header) that's directly following the current one in memory
static void handle_fd_message(const struct state *state, struct ipc_message *received, struct ipc_message *reply, struct jax_file *file, const void *next) {
    switch (FD_CALL_NUMBER(*received)) {
    case FD_READ:
        {
            void *data = (void *) syscall_invoke(FD_READ_BUFFER(*received).address, FD_READ_BUFFER(*received).depth, UNTYPED_LOCK, 0);

            if (data == NULL) {
                FD_RETURN_VALUE(*reply) = ENOMEM;
                break;
            }

            size_t size = syscall_invoke(FD_READ_BUFFER(*received).address, FD_READ_BUFFER(*received).depth, UNTYPED_SIZEOF, 0);

            FD_READ_BYTES_READ(*reply) = handle_read(state, received, reply, file, FD_READ_POSITION(*received), FD_READ_SIZE(*received) > size ? size : FD_READ_SIZE(*received), data);
            syscall_invoke(FD_READ_BUFFER(*received).address, FD_READ_BUFFER(*received).depth, UNTYPED_UNLOCK, 0);

            break;
        }
    case FD_READ_FAST:
        FD_READ_FAST_BYTES_READ(*reply) = handle_read(
            state,
            received,
            reply,
            file,
            FD_READ_FAST_POSITION(*received),
            FD_READ_FAST_SIZE(*received) > FD_READ_FAST_MAX_SIZE ? FD_READ_FAST_MAX_SIZE : FD_READ_FAST_SIZE(*received),
            FD_READ_FAST_DATA(*reply)
        );
        break;
    case FD_STAT:
        {
            struct stat *stat = &FD_STAT_STRUCT(*reply);
            stat->st_dev = 0;
            stat->st_ino = received->badge;
            stat->st_mode = file->mode;
            stat->st_nlink = 0;
            stat->st_uid = file->owner;
            stat->st_gid = file->group;
            stat->st_rdev = 0;
            stat->st_size = file->size;
            stat->st_atime = file->timestamp;
            stat->st_mtime = file->timestamp; // jax doesn't support these 2 fields without extensions that i don't give enough of a shit to design or implement right now
            stat->st_ctime = file->timestamp;
            stat->st_blksize = 512; // arbitrary, it doesn't matter lmao
            stat->st_blocks = (file->size + 511) / 512; // also arbitrary but it's probably good to give a value programs will at least expect

            FD_RETURN_VALUE(*reply) = 0;
            break;
        }
    case FD_OPEN:
        handle_open(state, received, reply, file, next);
        break;
    case FD_WRITE:
    case FD_WRITE_FAST:
    case FD_LINK:
    case FD_UNLINK:
    case FD_TRUNCATE:
        FD_RETURN_VALUE(*reply) = ENOTSUP;
        break;
    }
}

/// \brief handles an incoming ipc message based on whether it's for the root directory or not.
///
/// `received` points to the message that was just received, `reply` points to the message that will be sent as a reply.
/// it's the responsibility of whatever function calls this one to actually reply with the message, as that makes this function easily testable (no mockups required)
STATIC_TESTABLE void handle_ipc_message(const struct state *state, struct ipc_message *received, struct ipc_message *reply) {
    if (received->badge == 0) {
        // handle root directory case

        struct jax_file file = {
            .type = TYPE_DIRECTORY,
            .name_length = 0,
            .name = "",
            .description_length = 0,
            .description = "",
            .timestamp = 0,
            .mode = 0b101101101,
            .owner = 0,
            .group = 0,
            .size = 0,
            .data = NULL,
        };
        received->badge = (size_t) state->iterator.start;

        handle_fd_message(
            state,
            received,
            reply,
            &file,
            (const void *) state->iterator.start
        );
    } else {
        // handle non root directory case

        struct jax_file file;
        const void *next = next_file((const void *) received->badge, (const void *) state->initrd_end, &file);

        handle_fd_message(
            state,
            received,
            reply,
            &file,
            next
        );
    }
}

/// \brief the main loop of the program.
///
/// this handles receiving messages, handing them off to be processed, sending the reply, and cleaning up after replying
static void main_loop(const struct state *state) {
    struct ipc_message received = {
        .capabilities = {
            {(0 << INIT_NODE_DEPTH) | state->node.address, SIZE_MAX},
            {(1 << INIT_NODE_DEPTH) | state->node.address, SIZE_MAX},
            {(2 << INIT_NODE_DEPTH) | state->node.address, SIZE_MAX},
            {(3 << INIT_NODE_DEPTH) | state->node.address, SIZE_MAX}
        }
    };

    while (1) {
        received.badge = 0;
        size_t result = syscall_invoke(state->endpoint.address, SIZE_MAX, ENDPOINT_RECEIVE, (size_t) &received);

        if (result != 0) {
#ifdef UNDER_TEST
            // there needs to be a way to exit the main loop if this program is being tested, hence the break here
            break;
#else
            puts("initrd_fs: endpoint_receive failed with code ");
            print_number_hex(result);
            puts("\n");
            continue; // TODO: should this really continue? is this actually correct behavior?
#endif
        }

        debug_puts("initrd_fs: got fd call ");
        debug_print_number_hex(FD_CALL_NUMBER(received));
        debug_puts(" with badge ");
        debug_print_number_hex(received.badge);
        debug_puts("\n");

        struct ipc_message reply = {
            .capabilities = {}
        };

        handle_ipc_message(state, &received, &reply);
        syscall_invoke(FD_REPLY_ENDPOINT(received).address, SIZE_MAX, ENDPOINT_SEND, (size_t) &reply); // return value is ignored here since it doesn't matter if the reply fails to send

        // delete any leftover capabilities that were transferred and temporary capabilities not sent.
        for (size_t i = 0; i < IPC_CAPABILITY_SLOTS + 1; i ++) {
            syscall_invoke(state->node.address, state->node.depth, NODE_DELETE, i);
        }
    }
}

/// handles calling vfs_mount() to mount this filesystem at the root of the vfs
static void mount_to_root(const struct state *state) {
    // allocate the reply endpoint
    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 5,
        .depth = SIZE_MAX
    };
    assert(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args) == 0);

    // copy the file descriptor endpoint and set its badge to 0 so that operations on the root directory will be properly handled
    const struct node_copy_args fd_copy_args = {
        .source_address = state->endpoint.address,
        .source_depth = state->endpoint.depth,
        .dest_slot = IPC_CAPABILITY_SLOTS + 1,
        .access_rights = UINT8_MAX,
        .badge = 0,
        .should_set_badge = 1
    };
    assert(syscall_invoke(state->node.address, state->node.depth, NODE_COPY, (size_t) &fd_copy_args) == 0);

    // call vfs_mount()
    assert(fd_mount(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, (fd_copy_args.dest_slot << INIT_NODE_DEPTH) | state->node.address, MOUNT_REPLACE) == 0);

    debug_puts("initrd_fs: got here (after mount call)\n");
}

void _start(size_t initrd_start, size_t initrd_end) {
    debug_puts("hellorld from initrd_fs!\n");

    debug_puts("initrd_fs: initrd is at ");
    debug_print_number_hex(initrd_start);
    debug_puts(" to ");
    debug_print_number_hex(initrd_end);
    debug_puts("\n");

    struct jax_iterator iterator;

    if (!open_jax(&iterator, (const uint8_t *) initrd_start, (const uint8_t *) initrd_end)) {
        puts("initrd_fs: fatal: initrd format is invalid\n");

        while (true) {
            syscall_yield();
        }
    }

    // allocate a capability node to store received capabilities and temporary data
    const struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 3,
        .address = 3,
        .depth = INIT_NODE_DEPTH
    };
    assert(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args) == 0);

    // allocate the file descriptor endpoint that will be sent to the vfs and will be listened on for incoming messages
    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 4,
        .depth = SIZE_MAX
    };
    assert(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args) == 0);

    // this is used so that passing state around to the various functions here that use it is a lot cleaner
    const struct state state = {
        .initrd_start = initrd_start,
        .initrd_end = initrd_end,
        .iterator = iterator,
        .endpoint = (struct ipc_capability) {fd_alloc_args.address, fd_alloc_args.depth},
        .node = (struct ipc_capability) {node_alloc_args.address, node_alloc_args.depth}
    };

    mount_to_root(&state);
    main_loop(&state);
}
