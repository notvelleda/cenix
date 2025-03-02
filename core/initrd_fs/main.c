#include "jax.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "sys/stat.h"
#include "sys/vfs.h"

#define VFS_ENDPOINT_ADDRESS 2

// TODO: these really should be checked with an assert in release builds but i haven't bothered
#ifdef UNDER_TEST
#include "unity.h"
#define INVOKE_ASSERT(...) TEST_ASSERT(syscall_invoke(__VA_ARGS__) == 0)
#else
#define INVOKE_ASSERT(...) syscall_invoke(__VA_ARGS__)
#endif

static void print_number(size_t number) {
    // what can i say, i like writing fucked up for loops sometimes :3
    for (int i = sizeof(size_t) * 2 - 1; syscall_invoke(1, -1, DEBUG_PRINT, (size_t) &"0\0001\0002\0003\0004\0005\0006\0007\0008\0009\000a\000b\000c\000d\000e\000f"[((number >> (i * 4)) & 15) * 2]), i > 0; i --);
}

static const void *next_file(const void *address, const void *initrd_end, struct jax_file *file) {
    struct jax_iterator iterator = {
        .start = (const uint8_t *) address,
        .end = (const uint8_t *) initrd_end
    };

    return jax_next_file(&iterator, file) ? iterator.start : NULL;
}

/// gets the next file after the file pointed to by `address` in the given directory, placing its details in the `jax_file` struct pointed to by `file` and returning the file immediately following it if there aren't
/// any more files
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

/// handles a fd_read/fd_read_fast call
static size_t handle_read(size_t initrd_start, size_t initrd_end, struct ipc_message *received, struct ipc_message *reply, struct jax_file *file, size_t position, size_t read_size, void *read_data) {
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

        const void *next = next_file_in_dir(file, read_address, (const void *) initrd_start, (const void *) initrd_end, &current_file, &current_file_pointer);

        // TODO: make this emit . and .. directory entries, how would that work here?

        // this is done to make sure there'll be a next file after this directory entry
        if (next_file_in_dir(file, next, (const void *) initrd_start, (const void *) initrd_end, &temp_file, &temp_file_pointer) == NULL) {
            entry->next_entry_position = 0;
        } else {
            entry->next_entry_position = (size_t) next;
        }

        entry->inode = (ino_t) current_file_pointer;

        size_t name_length = current_file.name_length - file->name_length - 1;
        entry->name_length = (read_size - sizeof(struct vfs_directory_entry)) < name_length ? read_size - sizeof(struct vfs_directory_entry) : name_length;

        read_size = entry->name_length + sizeof(struct vfs_directory_entry);

        memcpy(entry->name, current_file.name + file->name_length + 1, entry->name_length);
    } else {
        // read from the file

        if (position >= file->size) {
            read_size = 0; // this case is to avoid overflow :3
        } else if (position + read_size >= file->size) {
            read_size = file->size - position;
        }

        memcpy(read_data, file->data + position, read_size);
    }

    FD_RETURN_VALUE(*reply) = 0;
    return read_size;
}

static void handle_open(
    size_t initrd_start,
    size_t initrd_end,
    struct ipc_capability *endpoint,
    struct ipc_capability *node,
    struct ipc_message *received,
    struct ipc_message *reply,
    struct jax_file *file,
    const void *next
) {
    if (file->type != TYPE_DIRECTORY) {
        FD_RETURN_VALUE(*reply) = ENOTSUP;
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
    while ((next = next_file_in_dir(file, next, (const void *) initrd_start, (const void *) initrd_end, &current_file, &current_file_pointer)) != NULL) {
        int i, j;

        for ((i = file->name_length == 0 ? 0 : (file->name_length + 1)), j = 0; i < current_file.name_length && current_file.name[i] == filename[j]; i ++, j ++);

        if (current_file.name[i] != filename[j]) {
            continue;
        }

        // found a matching file!

        // badge the endpoint with the raw address of the file
        const struct node_copy_args copy_args = {
            .source_address = endpoint->address,
            .source_depth = endpoint->depth,
            .dest_slot = IPC_CAPABILITY_SLOTS + 1,
            .access_rights = -1, // TODO: set access rights so that other processes can't listen on this endpoint
            .badge = (size_t) current_file_pointer,
            .should_set_badge = 1
        };
        size_t result = syscall_invoke(node->address, node->depth, NODE_COPY, (size_t) &copy_args);

        FD_RETURN_VALUE(*reply) = result;

        if (result == 0) {
            // send the newly badged endpoint back to the caller
            FD_OPEN_REPLY_FD(*reply).address = ((IPC_CAPABILITY_SLOTS + 1) << INIT_NODE_DEPTH) | node->address;
            FD_OPEN_REPLY_FD(*reply).depth = -1;
            FD_RETURN_VALUE(*reply) = 0;
        }

        syscall_invoke(FD_OPEN_NAME_ADDRESS(*received).address, FD_OPEN_NAME_ADDRESS(*received).depth, UNTYPED_UNLOCK, 0);
        return;
    }

    // no file was found :(
    FD_RETURN_VALUE(*reply) = ENOENT;
    syscall_invoke(FD_OPEN_NAME_ADDRESS(*received).address, FD_OPEN_NAME_ADDRESS(*received).depth, UNTYPED_UNLOCK, 0);
}

static void handle_fd_message(
    size_t initrd_start,
    size_t initrd_end,
    struct ipc_capability endpoint,
    struct ipc_capability node,
    struct ipc_message *received,
    struct ipc_message *reply,
    struct jax_file *file,
    const void *next
) {
    switch (FD_CALL_NUMBER(*received)) {
    case FD_READ:
        {
            void *data = (void *) syscall_invoke(FD_READ_BUFFER(*received).address, FD_READ_BUFFER(*received).depth, UNTYPED_LOCK, 0);

            if (data == NULL) {
                FD_RETURN_VALUE(*reply) = ENOMEM;
            } else {
                FD_READ_BYTES_READ(*reply) = handle_read(initrd_start, initrd_end, received, reply, file, FD_READ_POSITION(*received), FD_READ_SIZE(*received), data);
                syscall_invoke(FD_READ_BUFFER(*received).address, FD_READ_BUFFER(*received).depth, UNTYPED_UNLOCK, 0);
            }

            break;
        }
    case FD_READ_FAST:
        FD_READ_FAST_BYTES_READ(*reply) = handle_read(
            initrd_start,
            initrd_end,
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
        handle_open(initrd_start, initrd_end, &endpoint, &node, received, reply, file, next);
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

void _start(size_t initrd_start, size_t initrd_end) {
#ifdef DEBUG
    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "hellorld from initrd_fs!\n");

    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "initrd_fs: initrd is at ");
    print_number(initrd_start);
    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) " to ");
    print_number(initrd_end);
    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "\n");
#endif

    struct jax_iterator iterator;

    if (!open_jax(&iterator, (const uint8_t *) initrd_start, (const uint8_t *) initrd_end)) {
        INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "initrd_fs: fatal: initrd format is invalid\n");

#ifdef UNDER_TEST
        // provides a way to exit this function if this error condition is hit
        syscall_yield();
        return;
#else
        while (true) {
            syscall_yield();
        }
#endif
    }

    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    struct alloc_args path_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = 1,
        .address = 4,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &path_alloc_args);

    char *pointer = (char *) syscall_invoke(path_alloc_args.address, path_alloc_args.depth, UNTYPED_LOCK, 0);
    *pointer = '/';
    INVOKE_ASSERT(path_alloc_args.address, path_alloc_args.depth, UNTYPED_UNLOCK, 0);

    struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 5,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args);

    vfs_mount(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, path_alloc_args.address, fd_alloc_args.address, MREPL);
#ifdef DEBUG
    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "initrd_fs: got here (after mount call)\n");
#endif

    /*vfs_open_root(VFS_ENDPOINT_ADDRESS, endpoint_alloc_args.address, 6);
    INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "got here 2\n");*/

    // allocate a capability node to store received capabilities and temporary data
    struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 3,
        .address = 6,
        .depth = INIT_NODE_DEPTH
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args);

    struct ipc_message received = {
        .capabilities = {
            {(0 << INIT_NODE_DEPTH) | node_alloc_args.address, -1},
            {(1 << INIT_NODE_DEPTH) | node_alloc_args.address, -1},
            {(2 << INIT_NODE_DEPTH) | node_alloc_args.address, -1},
            {(3 << INIT_NODE_DEPTH) | node_alloc_args.address, -1}
        }
    };

    while (1) {
        received.badge = 0;
        size_t result = syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &received);

        if (result != 0) {
#ifdef UNDER_TEST
            // there needs to be a way to exit the main loop if this program is being tested, hence the break here
            break;
#else
            INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "initrd_fs: endpoint_receive failed with code ");
            print_number(result);
            INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "\n");
            continue;
#endif
        }

#ifdef DEBUG
        INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "initrd_fs: got fd call ");
        print_number(FD_CALL_NUMBER(received));
        INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) " with badge ");
        print_number(received.badge);
        INVOKE_ASSERT(1, -1, DEBUG_PRINT, (size_t) "\n");
#endif

        struct ipc_message reply = {
            .capabilities = {}
        };

        if (received.badge == 0) {
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
            received.badge = (size_t) iterator.start;

            handle_fd_message(
                initrd_start,
                initrd_end,
                (struct ipc_capability) {endpoint_alloc_args.address, endpoint_alloc_args.depth},
                (struct ipc_capability) {node_alloc_args.address, node_alloc_args.depth},
                &received,
                &reply,
                &file,
                (const void *) iterator.start
            );
        } else {
            // handle non root directory case

            struct jax_file file;
            const void *next = next_file((const void *) received.badge, (const void *) initrd_end, &file);

            handle_fd_message(
                initrd_start,
                initrd_end,
                (struct ipc_capability) {endpoint_alloc_args.address, endpoint_alloc_args.depth},
                (struct ipc_capability) {node_alloc_args.address, node_alloc_args.depth},
                &received,
                &reply,
                &file,
                next
            );
        }

        INVOKE_ASSERT(FD_REPLY_ENDPOINT(received).address, -1, ENDPOINT_SEND, (size_t) &reply);

        // delete any leftover capabilities that were transferred and temporary capabilities not sent
        for (size_t i = 0; i <= IPC_CAPABILITY_SLOTS; i ++) {
            if ((received.transferred_capabilities & (1 << i)) != 0) {
                syscall_invoke(node_alloc_args.address, node_alloc_args.depth, NODE_DELETE, i);
            }
        }
    }
}
