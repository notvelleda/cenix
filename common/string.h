#pragma once

#include <stddef.h>

void *memcpy(void *dst0, const void *src0, size_t length);
void *memmove(void *s1, const void *s2, size_t n);
void *memset(void *dest, int c, size_t n);

static inline int strcmp(const char *a, const char *b) {
    for (;; a ++, b ++) {
        if (*a < *b)
            return -1;
        if (*a > *b)
            return 1;
        if (*a == 0)
            return 0;
    }
}

static inline size_t strlen(const char *string) {
    size_t i = 0;
    for (; *string != 0; string ++, i ++);
    return i;
}
