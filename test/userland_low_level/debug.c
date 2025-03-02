#include "capabilities.h"
#include "debug.h"
#include <stdio.h>

static size_t debug_print(size_t address, size_t depth, struct capability *slot, size_t argument) {
    printf("%s", (char *) argument);
    return 0;
}

struct invocation_handlers debug_handlers = {
    .num_handlers = 1,
    .handlers = {debug_print}
};
