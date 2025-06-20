#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "capabilities.h"

struct heap {};

struct heap_header {
    bool is_locked;
    size_t size;
};

static inline void *heap_alloc(struct heap *heap, size_t actual_size) {
    (void) heap;

    struct heap_header *header = malloc(sizeof(struct heap_header) + actual_size);

    if (header == NULL) {
        return NULL;
    }

    header->is_locked = true;
    header->size = actual_size;

    return (uint8_t *) header + sizeof(struct heap_header);
}

static inline void heap_free(struct heap *heap, void *ptr) {
    (void) heap;

    return free(ptr - sizeof(struct heap_header));
}

static inline void heap_set_update_capability(void *ptr, const struct absolute_capability_address *address) {
    (void) ptr;
    (void) address;
}

static inline bool heap_lock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    if (!header->is_locked) {
        header->is_locked = true;
        return true;
    } else {
        return false;
    }
}

static inline void heap_unlock(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    header->is_locked = false;
}

static inline size_t heap_sizeof(void *ptr) {
    struct heap_header *header = (struct heap_header *) ((uint8_t *) ptr - sizeof(struct heap_header));
    return header->size;
}
