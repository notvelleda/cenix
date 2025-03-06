#pragma once

#ifdef UNDER_TEST
#include "unity.h"
#define INVOKE_ASSERT(...) TEST_ASSERT(syscall_invoke(__VA_ARGS__) == 0)
#define ASSERT_IF_TEST(...) TEST_ASSERT(__VA_ARGS__)
#define STATIC_TESTABLE
#else
#define INVOKE_ASSERT(...) syscall_invoke(__VA_ARGS__) // TODO: this should probably be checked with an assert of some kind in release builds but i haven't bothered
#define ASSERT_IF_TEST(...)
#define STATIC_TESTABLE static
#endif
