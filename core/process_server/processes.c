#include "bflt.h"
#include "core_io.h"
#include "errno.h"
#include "jax.h"
#include "processes.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "sys/limits.h"
#include "sys/types.h"

#define ALLOC_SLOT 0
#define PID_SET_SLOT 2
#define PID_THREAD_NODE_SLOT 3
#define PID_INFO_NODE_SLOT 4
#define PID_DATA_NODE_SLOT 5
#define VFS_ENDPOINT_SLOT 8

void init_process_table(void) {
    struct alloc_args set_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = PID_MAX / 8,
        .address = PID_SET_SLOT,
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &set_alloc_args);

    // TODO: use system pointer width for this
    uint32_t *pointer = (uint32_t *) syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_LOCK, 0);

    memset(pointer, 0, PID_MAX / 8);
    *pointer = 3; // pids 0 and 1 are reserved

    syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_UNLOCK, 0);

    struct alloc_args thread_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_THREAD_NODE_SLOT,
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &thread_node_alloc_args);

    struct alloc_args info_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_INFO_NODE_SLOT,
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &info_node_alloc_args);

    struct alloc_args data_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_DATA_NODE_SLOT,
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &data_node_alloc_args);
}

pid_t allocate_pid(void) {
    uint32_t *pointer = (uint32_t *) syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_LOCK, 0);

    if (pointer == NULL) {
        return 0;
    }

    pid_t pid = 0;

    for (int i = 0; i < PID_MAX / 32; i ++, pointer ++) {
        uint32_t value = *pointer; // TODO: should this be a size_t?

        if (value == 0xffffffff) {
            continue;
        }

        int bit_index;
        for (bit_index = 0; bit_index < 32 && (value & ((uint32_t) 1 << bit_index)) != 0; bit_index ++);

        *pointer |= ((uint32_t) 1 << bit_index);

        pid = i * 32 + bit_index;
        break;
    }

    syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_UNLOCK, 0);

    return pid;
}

void release_pid(pid_t pid) {
    uint32_t *pointer = (uint32_t *) syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_LOCK, 0);

    pointer[pid / 32] &= ~((uint32_t) 1 << (pid % 32));

    syscall_invoke(PID_SET_SLOT, SIZE_MAX, UNTYPED_UNLOCK, 0);
}

size_t exec_from_initrd(
    pid_t pid,
    struct jax_iterator *iter,
    const char *filename,
    size_t root_node_address,
    size_t root_node_depth,
    size_t (*registers_callback)(struct thread_registers *, void *),
    void *callback_data
) {
    const char *file_data;
    size_t file_size;
    if (!jax_find(iter, filename, TYPE_REGULAR, &file_data, &file_size)) {
        debug_printf("exec_from_initrd: couldn't find %s in initrd\n", filename);
        return ENOENT;
    }

    struct bflt_header *header = (struct bflt_header *) file_data;

    if (!bflt_verify(header)) {
        return ENOEXEC;
    }

    size_t allocation_size = bflt_allocation_size(header);

    debug_printf("exec_from_initrd: allocation size is %d\n", allocation_size);

    struct alloc_args data_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = allocation_size,
        .address = (pid << INIT_NODE_DEPTH) | PID_DATA_NODE_SLOT,
        .depth = SIZE_MAX
    };

    if (syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &data_alloc_args) != 0) {
        debug_printf("exec_from_initrd: memory allocation for thread data failed\n");
        return ENOMEM;
    }

    struct alloc_args thread_alloc_args = {
        .type = TYPE_THREAD,
        .size = 0,
        .address = (pid << INIT_NODE_DEPTH) | PID_THREAD_NODE_SLOT,
        .depth = SIZE_MAX
    };

    if (syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &thread_alloc_args) != 0) {
        debug_printf("exec_from_initrd: memory allocation for thread failed\n");

        syscall_invoke(PID_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, pid);
        return ENOMEM;
    }

    void *data = (void *) syscall_invoke(data_alloc_args.address, SIZE_MAX, UNTYPED_LOCK, 0);

    if (data == NULL) {
        debug_printf("exec_from_initrd: failed to lock thread data\n");

        syscall_invoke(PID_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, pid);
        syscall_invoke(PID_THREAD_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, pid);
        return ENOMEM;
    }

    struct thread_registers registers;
    bflt_load(header, data, &registers);

    size_t callback_result = registers_callback == NULL ? 0 : registers_callback(&registers, callback_data);

    if (callback_result != 0) {
        debug_printf("exec_from_initrd: registers_callback failed with code %d\n", callback_result);

        syscall_invoke(PID_DATA_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, pid);
        syscall_invoke(PID_THREAD_NODE_SLOT, INIT_NODE_DEPTH, NODE_DELETE, pid);
        return ENOMEM;
    }

    struct read_write_register_args register_write_args = {
        .address = &registers,
        .size = sizeof(struct thread_registers)
    };
    syscall_invoke(thread_alloc_args.address, SIZE_MAX, THREAD_WRITE_REGISTERS, (size_t) &register_write_args);

    struct set_root_node_args set_root_node_args = {root_node_address, root_node_depth};
    syscall_invoke(thread_alloc_args.address, SIZE_MAX, THREAD_SET_ROOT_NODE, (size_t) &set_root_node_args);

    syscall_invoke(thread_alloc_args.address, SIZE_MAX, THREAD_RESUME, 0);

    return 0;
}
