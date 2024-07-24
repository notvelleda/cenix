.macro save_registers
    moveml %a0-%a6/%d0-%d7, -(%sp) /* save all registers, even tho the c calling convention specifies some of these should already be saved it makes context switching easier */
    movel %usp, %a0 /* save user stack pointer */
    movel %a0, -(%sp)
.endm

.macro load_registers
    movel (%sp)+, %a0 /* load user stack pointer */
    movel %a0, %usp
    moveml (%sp)+, %a0-%a6/%d0-%d7 /* load all registers */
.endm

/* TODO: should USP be saved here? would it be easier to just save it/load it only on context switches? */

.macro trampoline, label, handler_label
.globl \label
\label:
    save_registers
    movel %sp, -(%sp)
    jsr \handler_label
    addql #4, %sp
    load_registers
    rte
.endm

trampoline bad_interrupt_entry, exception_handler
trampoline trap_entry, trap_handler
