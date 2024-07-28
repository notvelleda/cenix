#pragma once

#ifdef DEBUG
#include "printf.h"
#define printk(...) printf(__VA_ARGS__)
#include "capabilities.h"
extern struct invocation_handlers debug_handlers;
#else
#define printk(...)
#endif
