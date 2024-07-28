#pragma once

#include <stdint.h>
#include <stddef.h>

#define SYSCALL_YIELD 0
#define SYSCALL_INVOKE 1

static inline void syscall_yield(void) {
    register size_t kind asm ("d0") = SYSCALL_YIELD;
    __asm__ __volatile__ ("trap #0" :: "r" (kind));
}

static inline size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument) {
    register size_t return_value asm ("d0");
    register size_t kind asm ("d0") = SYSCALL_INVOKE;
    register size_t _address asm ("d1") = address;
    register size_t _depth asm ("d2") = depth;
    register size_t _handler_number asm ("d3") = handler_number;
    register size_t _argument asm ("a0") = argument;
    __asm__ __volatile__ ("trap #0" : "=r" (return_value) : "r" (kind), "r" (_address), "r" (_depth), "r" (_handler_number), "r" (_argument));
    return return_value;
}

/// the registers structure for the 68000 platform
struct thread_registers {
    uint32_t stack_pointer;
    uint32_t data[8];
    uint32_t address[7];
    uint16_t status_register; // most significant byte of this is discarded
    uint32_t program_counter;
};

static inline void set_program_counter(struct thread_registers *registers, size_t program_counter) {
    registers->program_counter = (uint32_t) program_counter;
}

static inline void set_stack_pointer(struct thread_registers *registers, size_t stack_pointer) {
    registers->stack_pointer = (uint32_t) stack_pointer;
}

static inline void set_got_pointer(struct thread_registers *registers, size_t got_pointer) {
    registers->address[5] = (uint32_t) got_pointer;
}
