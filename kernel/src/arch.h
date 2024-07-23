#pragma once

#ifdef ARCH_68000
#include "arch/68000/arch.h"
#endif

interrupt_status_t disable_interrupts(void);
void restore_interrupt_status(interrupt_status_t status);
