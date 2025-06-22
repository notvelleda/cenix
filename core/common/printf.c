#ifndef UNDER_TEST
#include "../printf/printf.c"
#include <stdint.h>
#include "sys/kernel.h"

void _putchar(char c) {
    char buffer[2] = {0, 0};
    buffer[0] = c;
    syscall_invoke(1, SIZE_MAX, DEBUG_PRINT, (size_t) &buffer);
}
#endif
