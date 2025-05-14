#pragma once

#include <stddef.h>
#include "sys/kernel.h"

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

#ifdef UNDER_TEST
#include "unity.h"
#define assert(condition) do {\
    if (!(condition)) {\
        TEST_FAIL_MESSAGE("assertion failed in " __FILE__ ":" STRINGIFY(__LINE__));\
    }\
} while (0)
#else
static inline size_t puts(const char *str) {
    return syscall_invoke(1, -1, DEBUG_PRINT, (size_t) str);
}

void __assertion_fail(const char *file, const char *line);

#define assert(condition) do {\
    if (!(condition)) {\
        __assertion_fail(__FILE__, STRINGIFY(__LINE__));\
    }\
} while (0)
#endif
