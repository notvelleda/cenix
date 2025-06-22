// memset from the Embedded Artistry Public Resources repository, under the CC0 license
// (though since i'm not sure if that applies given it's derived from musl i've included the relevant musl license below)
// https://github.com/embeddedartistry/embedded-resources/blob/9733be2eacdc3ede53b593dda9580ac21ca2283a/examples/libc/string/memset.c
//
// based on https://github.com/esmil/musl/blob/194f9cf93da8ae62491b7386edf481ea8565ae4e/src/string/memset.c or https://git.musl-libc.org/cgit/musl/tree/src/string/memset.c
//
// Copyright © 2005-2020 Rich Felker, et al.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "string.h"
#include <stdint.h>

void *memset(void *dest, int c, size_t n) {
    unsigned char *s = dest;
    size_t k;

    /* Fill head and tail with minimal branching. Each
     * conditional ensures that all the subsequently used
     * offsets are well-defined and in the dest region. */

    if (!n) {
        return dest;
    }

    s[0] = (unsigned char) c;
    s[n - 1] = (unsigned char) c;

    if (n <= 2) {
        return dest;
    }

    s[1] = (unsigned char) c;
    s[2] = (unsigned char) c;
    s[n - 2] = (unsigned char) c;
    s[n - 3] = (unsigned char) c;

    if (n <= 6) {
        return dest;
    }

    s[3] = (unsigned char) c;
    s[n - 4] = (unsigned char) c;

    if (n <= 8) {
        return dest;
    }

    /* Advance pointer to align it at a 4-byte boundary,
     * and truncate n to a multiple of 4. The previous code
     * already took care of any head/tail that get cut off
     * by the alignment. */

    k = -(uintptr_t) s & 3;
    s += k;
    n -= k;
    n &= (size_t) -4;
    n /= 4;

    // Cast to void first to prevent alignment warning
    uint32_t* ws = (uint32_t *) (void *) s;
    uint32_t wc = c & 0xFF;
    wc |= ((wc << 8) | (wc << 16) | (wc << 24));

    /* Pure C fallback with no aliasing violations. */
    for(; n; n--, ws++) {
        *ws = wc;
    }

    return dest;
}
