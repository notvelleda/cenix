#include <stdint.h>
#include "heap.h"
#include "debug.h"

#define KIND_AVAILABLE 0
#define KIND_IMMOVABLE 1
#define KIND_MOVABLE 2

const char *kind_names[] = {"available", "immovable", "movable"};

struct header {
    // the size of the memory block this header is for, including the size of the header
    size_t size;
    // what kind of header this is
    uint8_t kind;
    // a pointer to a pointer to the memory block associated with this header that should be updated when the header is moved to a new location
    void **update_ptr;

    // the next header in the list (TODO: combine this with `size`)
    struct header *next;
    // the previous header in the list
    struct header *prev;
};

static void split_header(struct header *header, size_t at) {
    // the header of the newly split block
    struct header *new_header = (struct header *) ((char *) header + at);
    new_header->size = header->size - at;
    new_header->kind = KIND_AVAILABLE;
    new_header->next = header->next;
    new_header->prev = header;

    if (new_header->next != NULL) {
        new_header->next->prev = new_header;
    }

    header->size = at;
    header->next = new_header;
}

void heap_init(struct heap *heap, struct init_block *init_block) {
    printk(
        "initializing heap at 0x%08x - 0x%08x (kernel at 0x%08x - 0x%08x)\n",
        init_block->memory_start,
        init_block->memory_end,
        init_block->kernel_start,
        init_block->kernel_end
    );

    if (init_block->memory_start >= init_block->kernel_start || init_block->memory_end <= init_block->kernel_end) {
        struct header *header = (struct header *) init_block->memory_start;
        header->kind = KIND_AVAILABLE;
        header->size = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
        header->prev = NULL;
        header->next = NULL;

        heap->heap_base = header;

        heap->total_memory = header->size;
        heap->used_memory = 0;
    } else {
        // TODO: properly handle heap at the very beginning or end of the memory block

        struct header *kernel_header = (struct header *) ((size_t) init_block->kernel_start - sizeof(struct header));
        kernel_header->kind = KIND_IMMOVABLE;
        kernel_header->size = (size_t) init_block->kernel_end - (size_t) kernel_header;

        struct header *header_low = (struct header *) init_block->memory_start;
        header_low->kind = KIND_AVAILABLE;
        header_low->size = (size_t) kernel_header - (size_t) header_low;

        heap->heap_base = header_low;

        struct header *header_high = (struct header *) init_block->kernel_end;
        header_high->kind = KIND_AVAILABLE;
        header_high->size = (size_t) init_block->memory_end - (size_t) header_high;

        header_low->prev = NULL;
        header_low->next = kernel_header;

        kernel_header->prev = header_low;
        kernel_header->next = header_high;

        header_high->prev = kernel_header;
        header_high->next = NULL;

        heap->total_memory = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
        heap->used_memory = kernel_header->size;
    }

    printk("total memory: %d KiB, used memory: %d KiB\n", heap->total_memory / 1024, heap->used_memory / 1024);
}

void heap_add_memory_block(struct heap *heap, void *start, void *end) {
    printk("adding memory to heap from 0x%08x - 0x%08x\n", start, end);
    // TODO: this
}

void *heap_alloc(struct heap *heap, size_t actual_size) {
    // TODO
    return NULL;
}

void heap_lock(void *ptr) {
    // TODO
}

void heap_unlock(void *ptr) {
    // TODO
}

void heap_free(struct heap *heap, void *ptr) {
    // TODO
}

void heap_list_blocks(struct heap *heap) {
    struct header *header = heap->heap_base;

    while (1) {
        printk(
            "0x%08x - 0x%08x (size %d (0x%x)): %s\n",
            header,
            (size_t) header + header->size,
            header->size,
            header->size,
            kind_names[header->kind]
        );

        if (header->next != NULL) {
            header = header->next;
        } else {
            break;
        }
    }
}
