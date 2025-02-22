#include "user_mode_entry.h"
#include "arch.h"
#include "scheduler.h"

void enter_user_mode(void *stack_pointer_origin) {
    struct thread_registers new_registers;

    scheduler_state.pending_context_switch = true; // force a context switch
    try_context_switch(&new_registers);

    // set user-mode stack pointer
    __asm__ __volatile__ ("movel %0, %%usp" :: "a" (new_registers.stack_pointer));

    uint16_t status_register = new_registers.status_register & 0x00ff;

    printk("bye-bye, supervisor mode!\n");

    // jump into user mode!
    __asm__ __volatile__ (
        "movel %0, %%sp\n\t"
        "movel %1, -(%%sp)\n\t"
        "movew %2, -(%%sp)\n\t"
        "moveml (%3)+, %%d0-%%d7/%%a0-%%a6\n\t"
        "rte"
        :: "r" (stack_pointer_origin), "r" (new_registers.program_counter), "r" (status_register), "r" (&new_registers.data)
    );

    while (1);
}
