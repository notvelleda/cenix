#include "debug_always.h"
#include <stdbool.h>
#include "sys/kernel.h"

void __assertion_fail(const char *file, const char *line) {
    puts("FATAL: assertion failed in ");
    puts(file);
    puts(":");
    puts(line);
    puts("\n");

    while (true) {
        syscall_yield();
    }
}
