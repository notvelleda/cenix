// memmove from the Embedded Artistry Public Resources repository, under the CC0 license
// https://github.com/embeddedartistry/embedded-resources/blob/9733be2eacdc3ede53b593dda9580ac21ca2283a/examples/libc/string/memmove.c

#include "string.h"

void *memmove(void *s1, const void *s2, size_t n) {
    return memcpy(s1, s2, n);
}
