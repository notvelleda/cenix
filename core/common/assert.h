#pragma once

#define __ASSERT_STRINGIFY(s) #s
#define _ASSERT_STRINGIFY(s) __ASSERT_STRINGIFY(s)

#ifdef UNDER_TEST
#include "unity.h"
#define assert(condition) do {\
    if (!(condition)) {\
        TEST_FAIL_MESSAGE("assertion failed in " __FILE__ ":" _ASSERT_STRINGIFY(__LINE__));\
    }\
} while (0)
#else
void __assertion_fail(const char *file, const char *line);

#define assert(condition) do {\
    if (!(condition)) {\
        __assertion_fail(__FILE__, _ASSERT_STRINGIFY(__LINE__));\
    }\
} while (0)
#endif
