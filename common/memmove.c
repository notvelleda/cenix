#include "string.h"

void *memmove(void *s1, const void *s2, size_t n) {
    return memcpy(s1, s2, n);
}
