#pragma once

#include <stdint.h>

struct registers {
    uint8_t condition_code_register;
    uint32_t program_counter;
    uint32_t data[8];
    uint32_t address[8];
};

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
