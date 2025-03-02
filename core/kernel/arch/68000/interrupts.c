#include "./arch.h"
#include "capabilities.h"
#include "debug.h"
#include "scheduler.h"
#include "sys/kernel.h"

extern void bad_interrupt_entry(void);
extern void bus_error_entry(void);
extern void address_error_entry(void);
extern void illegal_instruction_entry(void);
extern void division_by_zero_entry(void);
extern void chk_out_of_bounds_entry(void);
extern void division_by_zero_entry(void);
extern void trapv_overflow_entry(void);
extern void privilege_violation_entry(void);
extern void trace_entry(void);
extern void unimplemented_instruction_a_entry(void);
extern void unimplemented_instruction_f_entry(void);
extern void trap_entry(void);

void init_vector_table(void) {
    void **vector_table = (void **) 0;

    for (int i = 2; i < 64; i ++) {
        vector_table[i] = &bad_interrupt_entry;
    }

    vector_table[2] = &bus_error_entry;
    vector_table[3] = &address_error_entry;
    vector_table[4] = &illegal_instruction_entry;
    vector_table[5] = &division_by_zero_entry;
    vector_table[6] = &chk_out_of_bounds_entry;
    vector_table[7] = &trapv_overflow_entry;
    vector_table[8] = &privilege_violation_entry;
    vector_table[9] = &trace_entry;
    vector_table[10] = &unimplemented_instruction_a_entry;
    vector_table[11] = &unimplemented_instruction_f_entry;
    vector_table[32] = &trap_entry;
}

#ifdef DEBUG
static void log_registers(const struct thread_registers *registers) {
    printk(
        "d0: 0x%08x, d1: 0x%08x, d2: 0x%08x, d3: 0x%08x\n",
        registers->data[0],
        registers->data[1],
        registers->data[2],
        registers->data[3]
    );
    printk(
        "d4: 0x%08x, d5: 0x%08x, d6: 0x%08x, d7: 0x%08x\n",
        registers->data[4],
        registers->data[5],
        registers->data[6],
        registers->data[7]
    );
    printk(
        "a0: 0x%08x, a1: 0x%08x, a2: 0x%08x, a3: 0x%08x\n",
        registers->address[0],
        registers->address[1],
        registers->address[2],
        registers->address[3]
    );
    printk(
        "a4: 0x%08x, a5: 0x%08x, a6: 0x%08x, a7: 0x%08x\n",
        registers->address[4],
        registers->address[5],
        registers->address[6],
        registers->stack_pointer
    );
    printk("status register: 0x%04x, program counter: 0x%08x\n", registers->status_register, registers->program_counter);
}
#else
void _putchar(char c);

static void puts(const char *c) {
    for (; *c; c ++) {
        _putchar(*c);
    }
}
#endif

void handle_exception(struct thread_registers *registers, const char *cause) {
    if ((registers->status_register & (1 << 13)) == 0) {
        handle_thread_exception(registers, cause);
    } else {
#ifdef DEBUG
        printk("PANIC: unhandled exception \"%s\"\n", cause);
        log_registers(registers);
#else
        puts("PANIC: unhandled exception \"");
        puts(cause);
        puts("\"\n");
#endif
        while (1);
    }
}

void bad_interrupt_handler(struct thread_registers *registers) {
    handle_exception(registers, "bad interrupt");
}

void bus_error_handler(struct thread_registers *registers) {
    handle_exception(registers, "bus error");
}

void address_error_handler(struct thread_registers *registers) {
    handle_exception(registers, "address error");
}

void illegal_instruction_handler(struct thread_registers *registers) {
    handle_exception(registers, "illegal instruction");
}

void division_by_zero_handler(struct thread_registers *registers) {
    handle_exception(registers, "division by zero");
}

void chk_out_of_bounds_handler(struct thread_registers *registers) {
    handle_exception(registers, "CHK out of bounds");
}

void trapv_overflow_handler(struct thread_registers *registers) {
    handle_exception(registers, "TRAPV with overflow flag set");
}

void privilege_violation_handler(struct thread_registers *registers) {
    handle_exception(registers, "privilege violation");
}

void trace_handler(struct thread_registers *registers) {
    handle_exception(registers, "trace");
}

void unimplemented_instruction_a_handler(struct thread_registers *registers) {
    handle_exception(registers, "unimplemented instruction (line A)");
}

void unimplemented_instruction_f_handler(struct thread_registers *registers) {
    handle_exception(registers, "unimplemented instruction (line F)");
}

void trap_handler(struct thread_registers *registers) {
    switch (registers->data[0]) {
    case SYSCALL_YIELD:
        yield_thread();
        break;
    case SYSCALL_INVOKE:
        registers->data[0] = (uint32_t) invoke_capability(
            (size_t) registers->data[1],
            (size_t) registers->data[2],
            (size_t) registers->data[3],
            (size_t) registers->address[0]
        );
        break;
    }
    try_context_switch(registers);
}
