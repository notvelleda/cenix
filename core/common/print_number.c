#include "core_io.h"
#include <stddef.h>

void print_number_hex(size_t number) {
    // what can i say, i like writing fucked up for loops sometimes :3
    for (int i = sizeof(size_t) * 2 - 1; puts(&"0\0001\0002\0003\0004\0005\0006\0007\0008\0009\000a\000b\000c\000d\000e\000f"[((number >> (i * 4)) & 15) * 2]), i > 0; i --);
}
