#pragma once

static inline size_t syscall_invoke(size_t address, size_t depth, size_t handler_number, size_t argument) {
    register size_t _address asm ("d0") = address;
    register size_t _depth asm ("d1") = depth;
    register size_t _handler_number asm ("d2") = handler_number;
    register size_t _argument asm ("a0") = argument;
    register size_t return_value asm ("d0");
    __asm__ __volatile__ ("trap #0" : "=r" (return_value) : "r" (_address), "r" (_depth), "r" (_handler_number), "r" (_argument));
    return return_value;
}
