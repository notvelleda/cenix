#if defined(UNDER_TEST) && defined(DEBUG)
#include <stdio.h>
#elif defined(DEBUG)
#include "printf.h"
#else
#define printf(...)
#endif
