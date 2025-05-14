#pragma once

#ifdef UNDER_TEST
#include "unity.h"
#define ASSERT_IF_TEST(...) TEST_ASSERT(__VA_ARGS__)
#define STATIC_TESTABLE
#else
#define ASSERT_IF_TEST(...)
#define STATIC_TESTABLE static
#endif
