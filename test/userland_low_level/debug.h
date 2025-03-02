#pragma once

#include <stdio.h>
#define printk(...) printf("(printk) " __VA_ARGS__)

extern struct invocation_handlers debug_handlers;
