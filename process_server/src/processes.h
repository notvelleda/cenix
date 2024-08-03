#pragma once

#include <stdint.h>
#include "tar.h"

#define PID_MAX 1024
#define PID_BITS 9

typedef uint16_t pid_t;

void init_processes(void);
pid_t exec_from_initrd(struct tar_iterator *iter, char *filename);
