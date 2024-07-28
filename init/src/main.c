#include "sys/kernel.h"

void _start(void) {
    syscall_invoke(1, 4, DEBUG_PRINT, (size_t) "hello from init!");

    while (1) {
        syscall_yield();
    }
}
