#include <stdbool.h>
#include <stdint.h>
#include "sys/kernel.h"
#include "tar.h"

#ifdef DEBUG
#include "printf.h"
#else
#define printf(...)
#endif

#define STACK_SIZE 4096

extern const char _binary_initrd_tar_start;
extern const char _binary_initrd_tar_end;

void _start(void) {
    printf("hello from init!\n");

    size_t initrd_size = (size_t) &_binary_initrd_tar_end - (size_t) &_binary_initrd_tar_start;

    printf("initrd is at 0x%x to 0x%x, size %d\n", &_binary_initrd_tar_start, &_binary_initrd_tar_end, initrd_size);

    struct tar_iterator iter;
    open_tar(&iter, &_binary_initrd_tar_start, &_binary_initrd_tar_end);

    struct tar_header *header;
    const char *data;
    size_t size;

    while (next_file(&iter, &header, &data, &size)) {
        printf("found file %s\n", header->name);
    }

    while (1) {
        syscall_yield();
    }
}

#ifdef DEBUG
void _putchar(char c) {
    char buffer[2] = {0, 0};
    buffer[0] = c;
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) &buffer);
}
#endif
