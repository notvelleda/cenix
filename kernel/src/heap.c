#include <stdint.h>
#include "heap.h"
#include "debug.h"
#include "string.h"

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
    // don't bother splitting headers if there isn't enough space to fit a new one
    if (at >= header->size - sizeof(struct header)) {
        return;
    }

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
    size_t size = ((actual_size + sizeof(struct header)) + 3) & ~3;
    printk("alloc: size %d (adjusted to %d)\n", actual_size, size);

    // search for a series of consecutive movable or available blocks big enough to fit the allocation

    struct header *start_header = heap->heap_base;
    struct header *end_header = heap->heap_base;
    size_t total_size = heap->heap_base->size;
    size_t to_move = 0;
    size_t available_memory = heap->total_memory - heap->used_memory;

    if (size > available_memory) {
        return NULL;
    }

    while (1) {
        switch (end_header->kind) {
        case KIND_AVAILABLE:
            available_memory -= end_header->size;
            break;
        case KIND_MOVABLE:
            to_move += end_header->size;
            break;
        case KIND_IMMOVABLE:
            if (end_header->next != NULL) {
                // start the search over from the block following this immovable block
                start_header = end_header->next;
                end_header = end_header->next;
                total_size = end_header->size;
                to_move = 0;
                available_memory = heap->total_memory - heap->used_memory;
                continue;
            } else {
                return NULL;
            }
        default:
            break;
        }

        if (total_size >= size) {
            // found a potential series of blocks, make sure everything will work out
            if (to_move <= available_memory) {
                break;
            } else {
                // more data needs to be reallocated and moved around than there is available memory, attempt to find a different series of blocks
                start_header = end_header;
                total_size = end_header->size;
                to_move = 0;
                available_memory = heap->total_memory - heap->used_memory;
            }
        } else if (end_header->next != NULL) {
            end_header = end_header->next;
            total_size += end_header->next->size;
        } else {
            return NULL;
        }
    }

    printk("alloc: moving 0x%x, 0x%x available\n", to_move, available_memory);

    
    // split end_header
    size_t split_pos = size - (total_size - end_header->size);

    if (split_pos >= end_header->size) {
        split_pos = 0;
    }

    // only split the end header if it's unallocated and if the resulting block in the area to be allocated can actually fit data in it
    // if the allocation fails that makes it easier to clean up
    if (split_pos > sizeof(struct header) && end_header->kind == KIND_AVAILABLE) {
        printk("alloc: splitting end_header at %d\n", split_pos);
        split_header(end_header, split_pos);
    }

    int is_end_header_movable = end_header->kind == KIND_MOVABLE;

    if (to_move > 0) {
        for (struct header *header = start_header; header != end_header; header = header->next) {
            if (header->kind != KIND_MOVABLE) {
                continue;
            }

            const size_t alloc_size = header->size - sizeof(struct header);
            void *dest_ptr = heap_alloc(heap, alloc_size);

            if (dest_ptr == NULL) {
                return NULL;
            }

            // interrupts must be disabled since the original data can't be modified after it's copied but before any references to it are updated
            // TODO: figure out how to handle interrupt disabling/enabling in an architecture independent manner
            //arch.disable_interrupts();

            const void *src_ptr = (void *) ((char *) header + sizeof(struct header));
            memcpy(dest_ptr, src_ptr, alloc_size);

            // this is faster than just calling free() since alloc() will automatically merge consecutive blocks
            header->kind = KIND_AVAILABLE;
            heap->used_memory -= header->size;

            if (header->update_ptr != NULL) {
                *header->update_ptr = (void *) dest_ptr;
            }

            //arch.enable_interrupts();
        }
    }

    // since the contents of end_header have been properly moved, it can be split now
    if (split_pos > sizeof(struct header) && is_end_header_movable) {
        printk("alloc: splitting end_header at %d\n", split_pos);
        split_header(end_header, split_pos);
    }

    // set up the header and footer of the new allocation
    start_header->kind = KIND_IMMOVABLE;
    start_header->next = end_header->next;
    if (start_header->next != NULL) {
        start_header->next->prev = start_header;
    }

    heap->used_memory += size;

    return (void *) ((char *) start_header + sizeof(struct header));
}

void heap_lock(void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));
    header->kind = KIND_IMMOVABLE;
}

void heap_unlock(void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));
    header->kind = KIND_MOVABLE;
}

void heap_free(struct heap *heap, void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));

    header->kind = KIND_AVAILABLE;
    heap->used_memory -= header->size;

    // check if the block directly after this one is available, and merge them if it is
    if (header->next != NULL && header->next->kind == KIND_AVAILABLE) {
        header->size += header->next->size;
        header->next = header->next->next;

        if (header->next->next != NULL) {
            header->next->next->prev = header;
        }
    }

    // merge with the block directly before if applicable
    if (header->prev != NULL && header->prev->kind == KIND_AVAILABLE) {
        header->prev->size += header->size;
        header->prev->next = header->next;

        if (header->next != NULL) {
            header->next->prev = header->prev;
        }
    }
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
