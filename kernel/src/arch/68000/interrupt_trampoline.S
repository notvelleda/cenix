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

trampoline bad_interrupt_entry, bad_interrupt_handler
trampoline bus_error_entry, bus_error_handler
trampoline address_error_entry, address_error_handler
trampoline illegal_instruction_entry, illegal_instruction_handler
trampoline division_by_zero_entry, division_by_zero_handler
trampoline chk_out_of_bounds_entry, chk_out_of_bounds_handler
trampoline trapv_overflow_entry, trapv_overflow_handler
trampoline privilege_violation_entry, privilege_violation_handler
trampoline trace_entry, trace_handler
trampoline unimplemented_instruction_a_entry, unimplemented_instruction_a_handler
trampoline unimplemented_instruction_f_entry, unimplemented_instruction_f_handler
trampoline trap_entry, trap_handler
