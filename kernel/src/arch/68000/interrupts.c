#include "./arch.h"
#include "capabilities.h"
#include "scheduler.h"
#include "sys/kernel.h"

extern void trap_entry(void);
extern void bad_interrupt_entry(void);

void init_vector_table(void) {
    void **vector_table = (void **) 0;

    for (int i = 2; i < 64; i ++) {
        vector_table[i] = &bad_interrupt_entry;
    }
    vector_table[32] = &trap_entry;
}

static void log_registers(const struct thread_registers *registers) {
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
    printk("status register: 0x%04x, program counter: 0x%08x\n", registers->status_register, registers->program_counter);
}

void exception_handler(struct thread_registers *registers) {
    printk("something happened lol\n");
    log_registers(registers);
    while (1);
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
