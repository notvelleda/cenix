#include "debug.h"
#include "sys/kernel.h"

void _start(void) {
    printf("hello from vfs server!\n");

    while (1) {
        syscall_yield();
    }
}
