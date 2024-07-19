#pragma once

#ifdef DEBUG
#include "printf.h"
#define printk(...) printf(__VA_ARGS__)
#else
#define printk(...)
#endif
