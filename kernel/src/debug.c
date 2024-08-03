#include "debug.h"

static size_t debug_print(size_t address, size_t depth, struct capability *slot, size_t argument) {
#ifdef DEBUG
    printk("%s", (char *) argument);
#else
    void _putchar(char c);

    for (char *c = (char *) argument; *c; c ++) {
        _putchar(*c);
    }
#endif
    return 0;
}

struct invocation_handlers debug_handlers = {
    .num_handlers = 1,
    .handlers = {debug_print}
};
