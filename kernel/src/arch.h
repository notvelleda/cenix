#pragma once

#ifdef ARCH_68000
#include "arch/68000/arch.h"
#endif

#include <stddef.h>

interrupt_status_t disable_interrupts(void);
void restore_interrupt_status(interrupt_status_t status);

void set_program_counter(struct registers *registers, size_t program_counter);
void set_stack_pointer(struct registers *registers, size_t stack_pointer);
