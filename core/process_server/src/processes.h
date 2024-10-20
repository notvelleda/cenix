#pragma once

#include "jax.h"
#include <stddef.h>
#include "sys/types.h"

void init_process_table(void);

pid_t allocate_pid(void);
void release_pid(pid_t pid);

size_t exec_from_initrd(pid_t pid, struct jax_iterator *iter, const char *filename, size_t root_node_address, size_t root_node_depth);
