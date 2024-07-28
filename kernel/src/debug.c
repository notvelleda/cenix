#include "debug.h"

#ifdef DEBUG
static size_t debug_print(size_t address, size_t depth, struct capability *slot, size_t argument) {
    printk("%s", (char *) argument);
    return 1;
}

struct invocation_handlers debug_handlers = {
    .num_handlers = 1,
    .handlers = {debug_print}
};
#endif
