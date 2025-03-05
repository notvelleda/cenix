#pragma once

// this file is used so that test.c can access the state struct declaration and internal function declarations, not sure what it should be called really

#include "jax.h"
#include "sys/kernel.h"
#include "test_macros.h"

struct state {
    size_t initrd_start;
    size_t initrd_end;
    struct jax_iterator iterator;
    struct ipc_capability endpoint;
    struct ipc_capability node;
};

STATIC_TESTABLE void handle_ipc_message(const struct state *state, struct ipc_message *received, struct ipc_message *reply);
