#pragma once

#include "heap.h"
#include <stdint.h>

void init_console(uint16_t screen_width_from_bootloader, uint16_t screen_height_from_bootloader);
void lock_video_memory(struct heap *heap);
