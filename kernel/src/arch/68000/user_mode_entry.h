#pragma once

/// enters user mode, triggering a context switch in the process, and sets the stack pointer to the provided value
void enter_user_mode(void *stack_pointer_origin);
