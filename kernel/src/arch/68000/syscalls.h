#pragma once

#define SYSCALL_YIELD 0
#define SYSCALL_INVOKE 1

static inline void syscall_yield(void) {
    register size_t kind asm ("d0") = SYSCALL_YIELD;
    __asm__ __volatile__ ("trap #0" :: "r" (kind));
}

static inline size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument) {
    register size_t kind asm ("d0") = SYSCALL_INVOKE;
    register size_t _address asm ("d1") = address;
    register size_t _depth asm ("d2") = depth;
    register size_t _handler_number asm ("d3") = handler_number;
    register size_t _argument asm ("a0") = argument;
    register size_t return_value asm ("d0");
    __asm__ __volatile__ ("trap #0" : "=r" (return_value) : "r" (kind), "r" (_address), "r" (_depth), "r" (_handler_number), "r" (_argument));
    return return_value;
}
