#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#include "debug.h"
#include "heap.h"
#include "string.h"

#undef DEBUG_HEAP

static bool split_header(struct heap_header *header, size_t at) {
    // don't bother splitting headers if there isn't enough space to fit a new one
    if (at >= header->size - sizeof(struct heap_header)) {
        return false;
    } else if (at <= sizeof(struct heap_header)) {
        return false;
    }

    // the header of the newly split block
    struct heap_header *new_header = (struct heap_header *) ((char *) header + at);
    new_header->size = header->size - at;
    new_header->flags = KIND_AVAILABLE;
    new_header->next = header->next;
    new_header->prev = header;

    if (new_header->next != NULL) {
        new_header->next->prev = new_header;
    }

    header->size = at;
    header->next = new_header;

    return true;
}

void heap_init(struct heap *heap, struct init_block *init_block) {
    printk(
        "heap_init: initializing heap at 0x%08x - 0x%08x (kernel at 0x%08x - 0x%08x)\n",
        init_block->memory_start,
        init_block->memory_end,
        init_block->kernel_start,
        init_block->kernel_end
    );

    heap->total_memory = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
    heap->used_memory = 0;

    void *header_start = init_block->memory_start;
    void *header_end = header_start + sizeof(struct heap_header);

    // if the location of the initial heap header in memory overlaps with the kernel, move the start of the heap above the kernel
    // this can be handled by heap_lock_existing region, but then that would cause the start of the kernel to be overwritten
    if (header_end >= init_block->kernel_start && header_start < init_block->kernel_end) {
        heap->used_memory += (size_t) init_block->kernel_end - (size_t) init_block->memory_start;
        init_block->memory_start = init_block->kernel_end;
    }

    struct heap_header *header = (struct heap_header *) init_block->memory_start;
    header->flags = KIND_AVAILABLE;
    header->size = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
    header->prev = NULL;
    header->next = NULL;

    heap->heap_base = header;

    heap_lock_existing_region(heap, init_block->kernel_start, init_block->kernel_end);

    printk(
        "heap_init: total memory: %d KiB, used memory: %d KiB, free memory: %d KiB\n",
        heap->total_memory / 1024,
        heap->used_memory / 1024,
        (heap->total_memory - heap->used_memory) / 1024
    );
}

void heap_add_memory_block(struct heap *heap, void *start, void *end) {
#ifdef DEBUG_HEAP
    printk("heap: adding memory to heap from 0x%08x - 0x%08x\n", start, end);
#endif

    // TODO: this
}

void heap_lock_existing_region(struct heap *heap, void *start, void *end) {
    printk("heap_lock_existing_region: locking region from 0x%08x - 0x%08x\n", start, end);

    start -= sizeof(struct heap_header);

    for (struct heap_header *header = heap->heap_base; header != NULL; header = header->next) {
        void *block_start = (void *) header;
        void *block_end = block_start + header->size;

        if (block_end <= start || block_start >= end) {
            continue;
        } else if (GET_KIND(header) != KIND_AVAILABLE) {
            printk("heap_lock_existing_region: header at 0x%x isn't available and intersects with existing region\n", header);
            continue;
        }
        // TODO: should movable overlapping regions be moved? will that ever come up?

        if (start > block_start && start < block_end) {
            size_t at = (size_t) start - (size_t) block_start;

            if (at >= header->size - sizeof(struct heap_header)) {
                size_t offset = header->size - at;
                header->size = at;

                struct heap_header *next = header->next;
                if (next == NULL) {
                    continue;
                }

                if (GET_KIND(next) == KIND_AVAILABLE) {
                    struct heap_header tmp = *next;
                    tmp.size += offset;

                    next = (struct heap_header *) ((char *) header + at);
                    *next = tmp;
                    header->next = next;

                    continue;
                } else {
                    printk("heap_lock_existing_region: header at 0x%x isn't available and intersects with existing region\n", next);
                }

                continue;
            } else if (split_header(header, at)) {
                continue;
            }
        }

        if (end > block_start && end < block_end) {
            size_t at = (size_t) end - (size_t) block_start;

            if (at <= sizeof(struct heap_header) && header->prev != NULL) {
                struct heap_header *prev = header->prev;

                if (GET_KIND(prev) == KIND_AVAILABLE) {
                    prev->size -= at;

                    struct heap_header tmp = *header;
                    tmp.size += at;

                    header = (struct heap_header *) ((char *) header - at);
                    *header = tmp;
                    prev->next = header;

                    // try this header again
                    header = prev;
                    continue;
                } else {
                    printk("heap_lock_existing_region: header at 0x%x isn't available and intersects with existing region\n", prev);
                }
            } else {
                split_header(header, at);
            }
        }

        heap->used_memory += header->size;

        if (block_start + sizeof(struct heap_header) <= start + sizeof(struct heap_header) || end < block_start) {
            SET_KIND(header, KIND_IMMOVABLE);
            continue;
        }

        // this header lives inside of the locked region of memory, it's better for it to just Not Exist
        if (header->prev == NULL) {
            heap->heap_base = header->next;
        } else {
            header->prev->next = header->next;
        }

        if (header->next != NULL) {
            header->next->prev = header->prev;
        }
    }
}

#if defined(DEBUG) && defined(DEBUG_HEAP)
static size_t nesting = 0;

static void print_spaces(void) {
    for (size_t i = 0; i < nesting; i ++) {
        printk(" ");
    }
}
#endif

void *heap_alloc(struct heap *heap, size_t actual_size) {
    size_t size = ((actual_size + sizeof(struct heap_header)) + 3) & ~3;

#ifdef DEBUG_HEAP
    print_spaces();
    printk("heap_alloc: size %d (adjusted to %d)\n", actual_size, size);
#endif

    // search for a series of consecutive movable or available blocks big enough to fit the allocation

    struct heap_header *start_header = heap->heap_base;
    struct heap_header *end_header = heap->heap_base;
    size_t total_size = start_header->size;
    size_t to_move = 0;
    size_t available_memory = heap->total_memory - heap->used_memory;

    if (size > available_memory) {
        return NULL;
    }

    while (1) {
        switch (GET_KIND(end_header)) {
        case KIND_AVAILABLE:
            available_memory -= end_header->size;
            break;
        case KIND_MOVABLE:
            to_move += end_header->size;
            break;
        case KIND_IMMOVABLE:
            if (end_header->next == NULL) {
                return NULL;
            }

            // start the search over from the block following this immovable block
            start_header = end_header = end_header->next;
            total_size = start_header->size;
            to_move = 0;
            available_memory = heap->total_memory - heap->used_memory;
            continue;
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
                continue;
            }
        }

        if (end_header->next == NULL) {
            return NULL;
        }

        end_header = end_header->next;
        total_size += end_header->size;
    }

#ifdef DEBUG_HEAP
    print_spaces();
    printk("heap_alloc: moving 0x%x, 0x%x available\n", to_move, available_memory);
#endif

    // split end_header
    size_t split_pos = size - (total_size - end_header->size);

    if (split_pos >= end_header->size) {
        split_pos = 0;
    }

    // only split the end header if it's unallocated, if the allocation fails that makes it easier to clean up
    if (GET_KIND(end_header) == KIND_AVAILABLE) {
#ifdef DEBUG_HEAP
        print_spaces();
        printk("heap_alloc: splitting end_header at %d\n", split_pos);
#endif
        split_header(end_header, split_pos);
    }

    if (to_move > 0) {
        // mark all the headers that'll be occupied by this allocation as immovable,
        // and save their old kind values just in case this allocation fails
        // this is required so that allocated memory for newly moved headers doesn't lie inside the area taken up by the new allocation
        for (struct heap_header *header = start_header;; header = header->next) {
            uint8_t kind = GET_KIND(header);
            SET_OLD_KIND(header, kind);
            SET_KIND(header, KIND_IMMOVABLE);

            if (kind == KIND_AVAILABLE) {
                heap->used_memory += header->size;
            }

            if (header == end_header) {
                break;
            }
        }

        for (struct heap_header *header = start_header;; header = header->next) {
            // skip any headers that don't need to be moved
            if (GET_OLD_KIND(header) != KIND_MOVABLE) {
                if (header == end_header) {
                    break;
                } else {
                    continue;
                }
            }

            const size_t alloc_size = header->size - sizeof(struct heap_header);

#if defined(DEBUG) && defined(DEBUG_HEAP)
            nesting ++;
#endif
            void *dest_ptr = heap_alloc(heap, alloc_size);
#if defined(DEBUG) && defined(DEBUG_HEAP)
            nesting --;
#endif

            if (dest_ptr == NULL) {
                // allocation failed, revert any headers that haven't been moved to their old kind values
                for (struct heap_header *header = start_header;; header = header->next) {
                    uint8_t kind = GET_OLD_KIND(header);
                    SET_KIND(header, kind);

                    if (kind == KIND_AVAILABLE) {
                        heap->used_memory -= header->size;
                    }

                    if (header == end_header) {
                        break;
                    }
                }

                return NULL;
            }

            // interrupts must be disabled since the original data can't be modified after it's copied but before any references to it are updated
            interrupt_status_t status = disable_interrupts();

            const void *src_ptr = (void *) ((char *) header + sizeof(struct heap_header));
            memcpy(dest_ptr, src_ptr, alloc_size);

            SET_OLD_KIND(header, KIND_AVAILABLE);

            if ((header->flags & FLAG_CAPABILITY_RESOURCE) != 0) {
#ifdef DEBUG_HEAP
                print_spaces();
                printk(
                    "heap_alloc: updating capability resource at 0x%x:0x%x (%d bits) to 0x%x\n",
                    header->update_ref.capability.thread_id,
                    header->update_ref.capability.address,
                    header->update_ref.capability.depth,
                    header
                );
#endif
                update_capability_resource(&header->update_ref.capability, dest_ptr);
                heap_set_update_capability(dest_ptr, &header->update_ref.capability);
            } else if (header->update_ref.absolute_ptr != NULL) {
#ifdef DEBUG_HEAP
                print_spaces();
                printk("heap_alloc: updating absolute pointer at 0x%x to 0x%x\n", header->update_ref.absolute_ptr, header);
#endif
                *header->update_ref.absolute_ptr = (void *) dest_ptr;
                heap_set_update_absolute(dest_ptr, header->update_ref.absolute_ptr);
            }

            heap_unlock(dest_ptr);

            restore_interrupt_status(status);

            if (header == end_header) {
                break;
            }
        }
    }

    // set up the header and footer of the new allocation
    start_header->size = ((size_t) end_header + end_header->size) - (size_t) start_header;
    start_header->flags = KIND_IMMOVABLE;
    start_header->next = end_header->next;
    if (start_header->next != NULL) {
        start_header->next->prev = start_header;
    }

    if (to_move == 0) {
        heap->used_memory += start_header->size;
    }

    // try to split the newly created header to shave off any excess space
    if (split_header(start_header, size)) {
        struct heap_header *new_header = (struct heap_header *) ((char *) start_header + size);
        heap->used_memory -= new_header->size;
    }

    void *pointer = (void *) ((char *) start_header + sizeof(struct heap_header));

#ifdef DEBUG_HEAP
    print_spaces();
    printk("heap_alloc: returning pointer 0x%x\n", pointer);
#endif

    return pointer;
}

void heap_free(struct heap *heap, void *ptr) {
    struct heap_header *header = (struct heap_header *) ((char *) ptr - sizeof(struct heap_header));

    header->flags = KIND_AVAILABLE;
    heap->used_memory -= header->size;

    // check if the block directly after this one is available, and merge them if it is
    struct heap_header *next = header->next;
    if (next != NULL && GET_KIND(next) == KIND_AVAILABLE) {
        header->size += next->size;
        next = next->next;

        if (next->next != NULL) {
            next->next->prev = header;
        }
    }

    // merge with the block directly before if applicable
    struct heap_header *prev = header->prev;
    if (prev != NULL && GET_KIND(prev) == KIND_AVAILABLE) {
        prev->size += header->size;
        prev->next = header->next;

        if (header->next != NULL) {
            header->next->prev = prev;
        }
    }
}

#ifdef DEBUG
const char *kind_names[] = {"available", "immovable", "movable"};

void heap_list_blocks(struct heap *heap) {
    struct heap_header *header = heap->heap_base;

    while (1) {
        printk(
            "0x%08x - 0x%08x (size %d (0x%x)): %s\n",
            header,
            (size_t) header + header->size,
            header->size,
            header->size,
            kind_names[GET_KIND(header)]
        );

        if (header->next != NULL) {
            header = header->next;
        } else {
            break;
        }
    }
}
#endif
