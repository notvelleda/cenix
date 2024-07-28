#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sys/arch/68000.h"

typedef uint16_t interrupt_status_t; 

static inline interrupt_status_t disable_interrupts(void) {
    interrupt_status_t result;
    __asm__ __volatile__ (
        "movew %%sr, %0\n\t"
        "oriw #0x700, %%sr"
        : "=r" (result)
    );
    return result;
}

static inline void restore_interrupt_status(interrupt_status_t status) {
    __asm__ __volatile__ ("movew %0, %%sr" :: "r" (status));
}
