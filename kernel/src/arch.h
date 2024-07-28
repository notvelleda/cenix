#pragma once

#ifdef ARCH_68000
#include "sys/arch/68000.h"
#include "arch/68000/arch.h"
#endif

#include <stddef.h>

/// disables interrupts, returning a state object that must contain, among other things, whether interrupts were previously enabled or not
interrupt_status_t disable_interrupts(void);

/// \brief restores the interrupt state from a previously saved status
///
/// may enable interrupts or keep them disabled, depending on whether they were enabled at the time the status was saved or not
void restore_interrupt_status(interrupt_status_t status);

/// if there are any fields in a `thread_registers` object that user-mode code shouldn't mess with, this function sanitizes them
void sanitize_registers(struct thread_registers *registers);
