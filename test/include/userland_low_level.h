#pragma once

#include "capabilities.h"

// stubs that can be overrided by programs under test

size_t read_registers(size_t address, size_t depth, struct capability *slot, size_t argument);
size_t write_registers(size_t address, size_t depth, struct capability *slot, size_t argument);
size_t resume(size_t address, size_t depth, struct capability *slot, size_t argument);
size_t suspend(size_t address, size_t depth, struct capability *slot, size_t argument);
size_t set_root_node(size_t address, size_t depth, struct capability *slot, size_t argument);
void thread_destructor(struct capability *slot);

size_t endpoint_send(size_t address, size_t depth, struct capability *slot, size_t argument);
size_t endpoint_receive(size_t address, size_t depth, struct capability *slot, size_t argument);
void endpoint_destructor(struct capability *slot);

void syscall_yield(void);

void custom_setup(void);
void custom_teardown(void);

// utility functions

size_t read_badge(size_t address, size_t depth, size_t *badge);
