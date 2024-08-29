#include "debug.h"
#include "processes.h"
#include <stdbool.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "tar.h"

extern const char _binary_initrd_tar_start;
extern const char _binary_initrd_tar_end;

// TODO: consolidate the different definitions of this
#define INIT_NODE_DEPTH 4

void _start(void) {
    printf("hello from process server!\n");

    init_process_table();

    size_t initrd_size = (size_t) &_binary_initrd_tar_end - (size_t) &_binary_initrd_tar_start;

    printf("initrd is at 0x%x to 0x%x, size %d\n", &_binary_initrd_tar_start, &_binary_initrd_tar_end, initrd_size);

    struct tar_iterator iter;
    open_tar(&iter, &_binary_initrd_tar_start, &_binary_initrd_tar_end);

    /*struct tar_header *header;
    const char *data;
    size_t size;

    while (tar_next_file(&iter, &header, &data, &size)) {
        if (header->kind != TAR_NORMAL_FILE) {
            continue;
        }

        printf("found file %s\n", tar_get_name(header));
    }

    open_tar(&iter, &_binary_initrd_tar_start, &_binary_initrd_tar_end);*/

    struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 6,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args);

    struct alloc_args vfs_root_alloc_args = {
        .type = TYPE_NODE,
        .size = INIT_NODE_DEPTH,
        .address = 7,
        .depth = INIT_NODE_DEPTH
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &vfs_root_alloc_args);

    struct node_copy_args alloc_copy_args = {
        .source_address = 0,
        .source_depth = -1,
        .dest_slot = 0,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(vfs_root_alloc_args.address, vfs_root_alloc_args.depth, NODE_COPY, (size_t) &alloc_copy_args);

    struct node_copy_args debug_copy_args = {
        .source_address = 1,
        .source_depth = -1,
        .dest_slot = 1,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(vfs_root_alloc_args.address, vfs_root_alloc_args.depth, NODE_COPY, (size_t) &debug_copy_args);

    struct node_copy_args endpoint_copy_args = {
        .source_address = endpoint_alloc_args.address,
        .source_depth = endpoint_alloc_args.depth,
        .dest_slot = 2,
        .access_rights = -1,
        .badge = 0,
        .should_set_badge = 0
    };
    syscall_invoke(vfs_root_alloc_args.address, vfs_root_alloc_args.depth, NODE_COPY, (size_t) &endpoint_copy_args);

    pid_t vfs_pid = exec_from_initrd(&iter, "/sbin/vfs_server", vfs_root_alloc_args.address, vfs_root_alloc_args.depth);
    printf("vfs server is pid %d\n", vfs_pid);

    struct ipc_message message = {
        .capabilities = {{8, INIT_NODE_DEPTH}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(endpoint_alloc_args.address, endpoint_alloc_args.depth, ENDPOINT_RECEIVE, (size_t) &message);

    printf("got message! transferred_capabilities is 0x%x\n", message.transferred_capabilities);

    printf("sending response\n");

    struct ipc_message message2 = {
        .capabilities = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    };
    syscall_invoke(message.capabilities[0].address, message.capabilities[0].depth, ENDPOINT_SEND, (size_t) &message2);

    while (1) {
        syscall_yield();
    }
}
