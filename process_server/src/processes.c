#include "processes.h"
#include "bflt.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "sys/kernel.h"
#include "tar.h"

#define ALLOC_SLOT 0
#define PID_SET_SLOT 2
#define PID_THREAD_NODE_SLOT 3
#define PID_INFO_NODE_SLOT 4
#define PID_DATA_NODE_SLOT 5
#define INIT_NODE_DEPTH 4

void init_processes(void) {
    struct alloc_args set_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = PID_MAX / 8,
        .address = PID_SET_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &set_alloc_args);

    // TODO: use system pointer width for this
    uint32_t *pointer = (uint32_t *) syscall_invoke(PID_SET_SLOT, -1, UNTYPED_LOCK, 0);

    memset(pointer, PID_MAX / 8, 0);
    *pointer = 3; // pids 0 and 1 are reserved

    syscall_invoke(PID_SET_SLOT, -1, UNTYPED_UNLOCK, 0);

    struct alloc_args thread_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_THREAD_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &thread_node_alloc_args);

    struct alloc_args info_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_INFO_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &info_node_alloc_args);

    struct alloc_args data_node_alloc_args = {
        .type = TYPE_NODE,
        .size = PID_BITS,
        .address = PID_INFO_NODE_SLOT,
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &data_node_alloc_args);
}

static pid_t allocate_pid(void) {
    uint32_t *pointer = (uint32_t *) syscall_invoke(PID_SET_SLOT, -1, UNTYPED_LOCK, 0);

    if (pointer == NULL) {
        return 0;
    }

    pid_t pid = 0;

    for (int i = 0; i < PID_MAX / 32; i ++, pointer ++) {
        uint32_t value = *pointer;

        if (value == 0xffffffff) {
            continue;
        }

        int bit_index;
        for (bit_index = 0; bit_index < 32 && (value & (1 << bit_index)) != 0; bit_index ++);

        *pointer |= (1 << bit_index);

        pid = i * 32 + bit_index;
        break;
    }

    syscall_invoke(PID_SET_SLOT, -1, UNTYPED_UNLOCK, 0);

    return pid;
}

pid_t exec_from_initrd(struct tar_iterator *iter, char *filename) {
    pid_t pid = allocate_pid();

    if (pid == 0) {
        return 0;
    }

    const char *file_data;
    size_t file_size;
    if (!tar_find(iter, filename, TAR_NORMAL_FILE, &file_data, &file_size)) {
        // TODO: release pid
        return 0;
    }

    struct bflt_header *header = (struct bflt_header *) file_data;

    if (!bflt_verify(header)) {
        // TODO: release pid
        return 0;
    }

    size_t allocation_size = bflt_allocation_size(header);

    struct alloc_args data_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = allocation_size,
        .address = (pid << INIT_NODE_DEPTH) | PID_DATA_NODE_SLOT,
        .depth = -1
    };

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &data_alloc_args) != 0) {
        // TODO: release pid
        return 0;
    }

    struct alloc_args thread_alloc_args = {
        .type = TYPE_THREAD,
        .size = 0,
        .address = (pid << INIT_NODE_DEPTH) | PID_THREAD_NODE_SLOT,
        .depth = -1
    };

    if (syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &thread_alloc_args) != 0) {
        // TODO: release pid
        return 0;
    }

    void *data = (void *) syscall_invoke(data_alloc_args.address, -1, UNTYPED_LOCK, 0);

    if (data == NULL) {
        // TODO: release pid
        return 0;
    }

    struct thread_registers registers;
    bflt_load(header, data, &registers);

    struct read_write_register_args register_write_args = {
        .address = &registers,
        .size = sizeof(struct thread_registers)
    };
    syscall_invoke(thread_alloc_args.address, -1, THREAD_WRITE_REGISTERS, (size_t) &register_write_args);

    return pid;
}
