#include "core_io.h"
#include "early_init.h"
#include "processes.h"
#include <stdbool.h>
#include "sys/kernel.h"

void _start(void) {
    init_process_table();

    puts("running early init stage 1...\n");
    early_init();

    puts("done (for now)\n");
    while (true) {
        syscall_yield();
    }
}
