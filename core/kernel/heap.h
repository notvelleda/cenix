#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct heap {
    /// the header at the very start of this heap
    struct heap_header *heap_base;
    /// how much total memory is contained in this heap, in bytes
    size_t total_memory;
    /// how much memory has been used in this heap, in bytes
    size_t used_memory;
};

#include "capabilities.h"

/// the internal header for each memory region in the heap
struct heap_header {
    /// the size of the memory block this header is for, including the size of the header
    size_t size;
    /// flags describing what kind of header this is, among other things
    uint8_t flags;
    /// padding value to ensure contents of memory regions are aligned to 16 bits
    uint8_t padding;
    /// the address or handle that should be updated if the region this header controls is moved
    union {
        void **absolute_ptr;
        void (*function)(void *);
        struct absolute_capability_address capability;
    } update_ref;

    /// the next header in the list (TODO: combine this with `size`)
    struct heap_header *next;
    /// the previous header in the list
    struct heap_header *prev;
};

#define KIND_AVAILABLE 0
#define KIND_IMMOVABLE 1
#define KIND_MOVABLE 2
// old kind goes here (bits 3 and 4, flag values 4 and 8)
#define FLAG_CAPABILITY_RESOURCE 16
#define FLAG_UPDATE_FUNCTION 32

#define KIND_MASK 3
#define GET_KIND(header) (header->flags & (uint8_t) KIND_MASK)
#define SET_KIND(header, kind) { header->flags = (header->flags & (uint8_t) ~KIND_MASK) | kind; }
#define GET_OLD_KIND(header) ((header->flags >> 2) & (uint8_t) KIND_MASK)
#define SET_OLD_KIND(header, kind) { header->flags = (header->flags & (uint8_t) ~(KIND_MASK << 2)) | (kind << 2); }

/// \brief describes part of the system's memory map for heap initialization
///
/// describes enough of the system's memory map in order to initialize the heap.
/// further contiguous memory regions can be added to the heap with `heap_add_memory_block`
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

/// \brief initializes the heap
///
/// initializes the heap, allowing for allocations to be made in it.
/// the region of memory the heap initially occupies is specified by the parameter `init_block`,
/// and further regions of memory can be added with `heap_add_memory_block`
void heap_init(struct heap *heap, struct init_block *init_block);

/// adds a contiguous block of usable memory to the heap
void heap_add_memory_block(struct heap *heap, void *start, void *end);

/// locks an existing region of memory in the heap so that it won't be used for allocations
void heap_lock_existing_region(struct heap *heap, void *start, void *end);

/// allocates a region of memory, returning a pointer to it. the newly allocated region of memory is set as locked (immovable)
void *heap_alloc(struct heap *heap, size_t actual_size);

// TODO: add heap_realloc so things like capability nodes can have their slots allocated on-demand (which will drastically reduce memory usage and increase performance)
// for best performance, realloc should ideally make a new allocation if the cost of copying any overlapping allocations is greater than the cost of copying the existing data (when allocation is optimized that is)

/// \brief locks an allocated region of memory in place, allowing for any pointers to it to remain valid
///
/// if the returned value is true, this region of memory wasn't locked beforehand.
/// if the returned value is false, this region of memory was locked
/// TODO: will any memory regions end up being locked multiple times in practice? is this worth the extra few cycles?
static inline bool heap_lock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    if (GET_KIND(header) == KIND_MOVABLE) {
        SET_KIND(header, KIND_IMMOVABLE);
        return true;
    } else {
        return false;
    }
}

/// unlocks an allocated region of memory, invalidating any existing pointers to it and allowing it to be moved anywhere else in memory if required
static inline void heap_unlock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    SET_KIND(header, KIND_MOVABLE);
}

/// frees a region of memory, allowing it to be reused for other things
void heap_free(struct heap *heap, void *ptr);

/// \brief gets the size of the object at the given address
///
/// if the address doesn't correspond to a valid object allocated on a heap, the returned value is undefined
static inline size_t heap_sizeof(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    return header->size - sizeof(struct heap_header);
}

#ifdef DEBUG
/// prints out a list of all the blocks in the heap
void heap_list_blocks(struct heap *heap);
#endif

/// sets the absolute address that should be updated if the given memory region is moved.
/// this update address will replace any addresses, handles, or functions set previously
static inline void heap_set_update_absolute(void *ptr, void **absolute_ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));

    // TODO: this section is probably critical, should interrupts be disabled?
    header->flags &= (uint8_t) ~(FLAG_CAPABILITY_RESOURCE | FLAG_UPDATE_FUNCTION);
    header->update_ref.absolute_ptr = absolute_ptr;
}

/// sets the function that should be called if the given memory region is moved.
/// this update function will replace any addresses, handles, or functions set previously
static inline void heap_set_update_function(void *ptr, void (*function)(void *)) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));

    // TODO: this section is probably critical, should interrupts be disabled?
    header->flags &= (uint8_t) ~FLAG_CAPABILITY_RESOURCE;
    header->flags |= (uint8_t) FLAG_UPDATE_FUNCTION;
    header->update_ref.function = function;
}

#include "capabilities.h"

/// sets the address in capability space of the capability that should be updated if the given memory region is moved.
/// this capability address will replace any absolute addresses, capability addresses, or functions set previously
static inline void heap_set_update_capability(void *ptr, const struct absolute_capability_address *address) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));

    // TODO: this section is probably critical, should interrupts be disabled?
    header->flags &= (uint8_t) ~FLAG_UPDATE_FUNCTION;
    header->flags |= (uint8_t) FLAG_CAPABILITY_RESOURCE;
    header->update_ref.capability = *address;
}
