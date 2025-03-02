#include "fff.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "sys/kernel.h"
#include "unity.h"
#include "userland_low_level_stubs.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(syscall_yield);

void entry_point(size_t initrd_start, size_t initrd_end);

void custom_setup(void) {
    RESET_FAKE(syscall_yield);

    FFF_RESET_HISTORY();
}

void initrd_as_null(void) {
    entry_point(0, 0);
    TEST_ASSERT(syscall_yield_fake.call_count == 1);
}

void initrd_as_random_data(void) {
    char *random_data = "qwertyuiopasdfghjklzxcvbnm";
    entry_point((size_t) random_data, (size_t) random_data + strlen(random_data));
    TEST_ASSERT(syscall_yield_fake.call_count == 1);
}

// TODO: properly unit test the rest of this program

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(initrd_as_null);
    RUN_TEST(initrd_as_random_data);
    return UNITY_END();
}
