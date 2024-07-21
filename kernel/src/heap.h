#pragma once

#include <stddef.h>

/// describes how the memory that the kernel is loaded in is laid out
struct init_block {
    /// a pointer to the start of the kernel (inclusive)
    void *kernel_start;
    /// a pointer to the end of the kernel (exclusive)
    void *kernel_end;
    /// a pointer to the start of a contiguous block of memory (inclusive)
    void *memory_start;
    /// a pointer to the end of the contiguous block of memory (exclusive)
    void *memory_end;
};

struct heap {
    struct header *heap_base;
    size_t total_memory;
    size_t used_memory;
};

void heap_init(struct heap *heap, struct init_block *init_block);

// adds a contiguous block of usable memory to the heap
void heap_add_memory_block(struct heap *heap, void *start, void *end);

// locks an existing region of memory in the heap so that it won't be used for allocations
void heap_lock_existing_region(struct heap *heap, void *start, void *end);

// allocates a region of memory, returning a pointer to it. the newly allocated region of memory is set as locked (immovable)
void *heap_alloc(struct heap *heap, size_t actual_size);

// locks an allocated region of memory in place, allowing for any pointers to it to remain valid
void heap_lock(void *ptr);

// unlocks an allocated region of memory, invalidating any existing pointers to it and allowing it to be moved anywhere else in memory if required
void heap_unlock(void *ptr);

// frees a region of memory, allowing it to be reused for other things
void heap_free(struct heap *heap, void *ptr);

// prints out a list of all the blocks in the heap
void heap_list_blocks(struct heap *heap);

// sets the absolute address that should be updated if the given memory region is moved
// this update address will replace any addresses or handles set previously
void heap_set_update_absolute(void *ptr, void **absolute_ptr);

#include "kernel_memory.h"

// sets the kernel memory handle that should be updated if the given memory region is moved
// this handle will replace any absolute addresses or handles set previously
void heap_set_update_handle(void *ptr, kmem_handle_t handle);
