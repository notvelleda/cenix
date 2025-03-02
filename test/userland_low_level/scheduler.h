#pragma once

struct scheduler_state {
    struct thread_capability *current_thread;
};

extern struct scheduler_state scheduler_state;
