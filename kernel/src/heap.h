#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct heap_header {
    // the size of the memory block this header is for, including the size of the header
    size_t size;
    // flags describing what kind of header this is, among other things
    uint8_t flags;
    // the address or handle that should be updated if the region this header controls is moved
    union {
        void **absolute_ptr;
        struct {
            size_t address;
            size_t depth;
        } capability;
    } update_ref;
    // how many references exist to this header
    size_t num_references;

    // the next header in the list (TODO: combine this with `size`)
    struct heap_header *next;
    // the previous header in the list
    struct heap_header *prev;
};

#define KIND_AVAILABLE 0
#define KIND_IMMOVABLE 1
#define KIND_MOVABLE 2
// old kind goes here (bits 3 and 4, flag values 4 and 8)
#define FLAG_CAPABILITY_RESOURCE 16

#define KIND_MASK 3
#define GET_KIND(header) (header->flags & KIND_MASK)
#define SET_KIND(header, kind) { header->flags = (header->flags & ~KIND_MASK) | kind; }
#define GET_OLD_KIND(header) ((header->flags >> 2) & KIND_MASK)
#define SET_OLD_KIND(header, kind) { header->flags = (header->flags & ~(KIND_MASK << 2)) | (kind << 2); }

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
    struct heap_header *heap_base;
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

// locks an allocated region of memory in place, allowing for any pointers to it to remain valid.
// if the returned value is true, this region of memory wasn't locked beforehand.
// if the returned value is false, this region of memory was locked
// TODO: will any memory regions end up being locked multiple times in practice? is this worth the extra few cycles?
inline bool heap_lock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((char *) ptr - sizeof(struct heap_header));
    if (GET_KIND(header) == KIND_MOVABLE) {
        SET_KIND(header, KIND_IMMOVABLE);
        return true;
    } else {
        return false;
    }
}

// unlocks an allocated region of memory, invalidating any existing pointers to it and allowing it to be moved anywhere else in memory if required
inline void heap_unlock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((char *) ptr - sizeof(struct heap_header));
    SET_KIND(header, KIND_MOVABLE);
}

// frees a region of memory, allowing it to be reused for other things
void heap_free(struct heap *heap, void *ptr);

// increments the reference count of the provided region of memory
inline void heap_add_reference(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((char *) ptr - sizeof(struct heap_header));
    header->num_references ++;
}

// decrements the reference count of the provided region of memory, freeing it if the reference count is 0
inline void heap_remove_reference(struct heap *heap, void *ptr) {
    struct heap_header *header = (struct heap_header *) ((char *) ptr - sizeof(struct heap_header));
    header->num_references --;

    if (header->num_references == 0) {
        heap_free(heap, ptr);
    }
}

// prints out a list of all the blocks in the heap
void heap_list_blocks(struct heap *heap);

// sets the absolute address that should be updated if the given memory region is moved
// this update address will replace any addresses or handles set previously
void heap_set_update_absolute(void *ptr, void **absolute_ptr);

#include "capabilities.h"

// sets the address in capability space of the capability that should be updated if the given memory region is moved
// this capability address will replace any absolute addresses or capability addresses set previously
void heap_set_update_capability(void *ptr, size_t address, size_t depth);
