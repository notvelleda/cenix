#pragma once

#ifdef UNDER_TEST
#include <stdio.h>
// i don't like that puts appends newlines. i know this is nonstandard and i couldn't care less
#define puts(string) printf("%s", string)
#else
#include "../printf/printf.h"
#include <stddef.h>
#include "sys/kernel.h"

static inline size_t puts(const char *str) {
    return syscall_invoke(1, -1, DEBUG_PRINT, (size_t) str);
}
#endif

/// prints a number to the debug console in hexadecimal form
void print_number_hex(size_t number);

#ifdef DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#define debug_puts(string) puts(string)
#define debug_print_number_hex(number) print_number_hex(number)
#else
#define debug_printf(...)
#define debug_puts(string)
#define debug_print_number_hex(number)
#endif
