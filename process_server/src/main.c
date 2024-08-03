#include "debug.h"
#include "processes.h"
#include <stdbool.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "tar.h"

extern const char _binary_initrd_tar_start;
extern const char _binary_initrd_tar_end;

void _start(void) {
    printf("hello from process server!\n");

    init_processes();

    size_t initrd_size = (size_t) &_binary_initrd_tar_end - (size_t) &_binary_initrd_tar_start;

    printf("initrd is at 0x%x to 0x%x, size %d\n", &_binary_initrd_tar_start, &_binary_initrd_tar_end, initrd_size);

    struct tar_iterator iter;
    open_tar(&iter, &_binary_initrd_tar_start, &_binary_initrd_tar_end);

    struct tar_header *header;
    const char *data;
    size_t size;

    while (tar_next_file(&iter, &header, &data, &size)) {
        if (header->kind != TAR_NORMAL_FILE) {
            continue;
        }

        printf("found file %s\n", tar_get_name(header));
    }

    open_tar(&iter, &_binary_initrd_tar_start, &_binary_initrd_tar_end);

    pid_t vfs_pid = exec_from_initrd(&iter, "/sbin/vfs_server");
    printf("vfs is pid %d\n", vfs_pid);

    while (1) {
        syscall_yield();
    }
}

void _putchar(char c) {
    char buffer[2] = {0, 0};
    buffer[0] = c;
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &buffer);
}
