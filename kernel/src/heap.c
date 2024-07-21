#include <stdint.h>
#include <stdbool.h>
#include "heap.h"
#include "debug.h"
#include "string.h"

#define KIND_AVAILABLE 0
#define KIND_IMMOVABLE 1
#define KIND_MOVABLE 2
// old kind goes here (bits 3 and 4, flag values 4 and 8)
#define FLAG_KMEM_UPDATE 16

#define KIND_MASK 3
#define GET_KIND(header) (header->flags & KIND_MASK)
#define SET_KIND(header, kind) { header->flags = (header->flags & ~KIND_MASK) | kind; }
#define GET_OLD_KIND(header) ((header->flags >> 2) & KIND_MASK)
#define SET_OLD_KIND(header, kind) { header->flags = (header->flags & ~(KIND_MASK << 2)) | (kind << 2); }

const char *kind_names[] = {"available", "immovable", "movable"};

struct header {
    // the size of the memory block this header is for, including the size of the header
    size_t size;
    // flags describing what kind of header this is, among other things
    uint8_t flags;
    // the address or handle that should be updated if the region this header controls is moved
    union {
        void **absolute_ptr;
        kmem_handle_t handle;
    } update_ref;

    // the next header in the list (TODO: combine this with `size`)
    struct header *next;
    // the previous header in the list
    struct header *prev;
};

static bool split_header(struct header *header, size_t at) {
    // don't bother splitting headers if there isn't enough space to fit a new one
    if (at >= header->size - sizeof(struct header)) {
        return false;
    } else if (at <= sizeof(struct header)) {
        return false;
    }

    // the header of the newly split block
    struct header *new_header = (struct header *) ((char *) header + at);
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
        "initializing heap at 0x%08x - 0x%08x (kernel at 0x%08x - 0x%08x)\n",
        init_block->memory_start,
        init_block->memory_end,
        init_block->kernel_start,
        init_block->kernel_end
    );

    heap->total_memory = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
    heap->used_memory = 0;

    void *header_start = init_block->memory_start;
    void *header_end = header_start + sizeof(struct header);

    // if the location of the initial heap header in memory overlaps with the kernel, move the start of the heap above the kernel
    // this can be handled by heap_lock_existing region, but then that would cause the start of the kernel to be overwritten
    if (header_end >= init_block->kernel_start && header_start < init_block->kernel_end) {
        heap->used_memory += (size_t) init_block->kernel_end - (size_t) init_block->memory_start;
        init_block->memory_start = init_block->kernel_end;
    }

    struct header *header = (struct header *) init_block->memory_start;
    header->flags = KIND_AVAILABLE;
    header->size = (size_t) init_block->memory_end - (size_t) init_block->memory_start;
    header->prev = NULL;
    header->next = NULL;

    heap->heap_base = header;

    heap_lock_existing_region(heap, init_block->kernel_start, init_block->kernel_end);

    printk(
        "total memory: %d KiB, used memory: %d KiB, free memory: %d KiB\n",
        heap->total_memory / 1024,
        heap->used_memory / 1024,
        (heap->total_memory - heap->used_memory) / 1024
    );
}

void heap_add_memory_block(struct heap *heap, void *start, void *end) {
    printk("adding memory to heap from 0x%08x - 0x%08x\n", start, end);
    // TODO: this
}

void heap_lock_existing_region(struct heap *heap, void *start, void *end) {
    printk("locking region from 0x%08x - 0x%08x\n", start, end);

    start -= sizeof(struct header);

    for (struct header *header = heap->heap_base; header != NULL; header = header->next) {
        void *block_start = (void *) header;
        void *block_end = block_start + header->size;

        if (block_end <= start || block_start >= end) {
            continue;
        } else if (GET_KIND(header) != KIND_AVAILABLE) {
            printk("header at 0x%x isn't available and intersects with existing region\n", header);
            continue;
        }
        // TODO: should movable overlapping regions be moved? will that ever come up?

        if (start > block_start && start < block_end) {
            size_t at = (size_t) start - (size_t) block_start;

            if (at >= header->size - sizeof(struct header)) {
                size_t offset = header->size - at;
                header->size = at;

                struct header *next = header->next;
                if (next == NULL) {
                    continue;
                }

                if (GET_KIND(next) == KIND_AVAILABLE) {
                    struct header tmp = *next;
                    tmp.size += offset;

                    next = (struct header *) ((char *) header + at);
                    *next = tmp;
                    header->next = next;

                    continue;
                } else {
                    printk("header at 0x%x isn't available and intersects with existing region\n", next);
                }

                continue;
            } else if (split_header(header, at)) {
                continue;
            }
        }

        if (end > block_start && end < block_end) {
            size_t at = (size_t) end - (size_t) block_start;

            if (at <= sizeof(struct header) && header->prev != NULL) {
                struct header *prev = header->prev;

                if (GET_KIND(prev) == KIND_AVAILABLE) {
                    prev->size -= at;

                    struct header tmp = *header;
                    tmp.size += at;

                    header = (struct header *) ((char *) header - at);
                    *header = tmp;
                    prev->next = header;

                    // try this header again
                    header = prev;
                    continue;
                } else {
                    printk("header at 0x%x isn't available and intersects with existing region\n", prev);
                }
            } else {
                split_header(header, at);
            }
        }

        heap->used_memory += header->size;

        if (block_start + sizeof(struct header) <= start + sizeof(struct header) || end < block_start) {
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

#ifdef DEBUG
static size_t nesting = 0;

static void print_spaces(void) {
    for (size_t i = 0; i < nesting; i ++) {
        printk(" ");
    }
}
#else
#define print_spaces()
#endif

void *heap_alloc(struct heap *heap, size_t actual_size) {
    size_t size = ((actual_size + sizeof(struct header)) + 3) & ~3;
    print_spaces();
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
        switch (GET_KIND(end_header)) {
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

    print_spaces();
    printk("alloc: moving 0x%x, 0x%x available\n", to_move, available_memory);

    // split end_header
    size_t split_pos = size - (total_size - end_header->size);

    if (split_pos >= end_header->size) {
        split_pos = 0;
    }

    // only split the end header if it's unallocated, if the allocation fails that makes it easier to clean up
    if (GET_KIND(end_header) == KIND_AVAILABLE) {
        print_spaces();
        printk("alloc: splitting end_header at %d\n", split_pos);
        split_header(end_header, split_pos);
    }

    if (to_move > 0) {
        // mark all the headers that'll be occupied by this allocation as immovable,
        // and save their old kind values just in case this allocation fails
        // this is required so that allocated memory for newly moved headers doesn't lie inside the area taken up by the new allocation
        for (struct header *header = start_header;; header = header->next) {
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

        for (struct header *header = start_header;; header = header->next) {
            // skip any headers that don't need to be moved
            if (GET_OLD_KIND(header) != KIND_MOVABLE) {
                if (header == end_header) {
                    break;
                } else {
                    continue;
                }
            }

            const size_t alloc_size = header->size - sizeof(struct header);

#ifdef DEBUG
            nesting ++;
#endif
            void *dest_ptr = heap_alloc(heap, alloc_size);
#ifdef DEBUG
            nesting --;
#endif

            if (dest_ptr == NULL) {
                // allocation failed, revert any headers that haven't been moved to their old kind values
                for (struct header *header = start_header;; header = header->next) {
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
            // TODO: figure out how to handle interrupt disabling/enabling in an architecture independent manner
            //arch.disable_interrupts();

            const void *src_ptr = (void *) ((char *) header + sizeof(struct header));
            memcpy(dest_ptr, src_ptr, alloc_size);

            SET_OLD_KIND(header, KIND_AVAILABLE);

            if ((header->flags & FLAG_KMEM_UPDATE) != 0) {
                kmem_update(header->update_ref.handle, (void *) dest_ptr);
                heap_set_update_handle(dest_ptr, header->update_ref.handle);
            } else if (header->update_ref.absolute_ptr != NULL) {
                *header->update_ref.absolute_ptr = (void *) dest_ptr;
                heap_set_update_absolute(dest_ptr, header->update_ref.absolute_ptr);
            }

            heap_unlock(dest_ptr);

            //arch.enable_interrupts();

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
        struct header *new_header = (struct header *) ((char *) start_header + size);
        heap->used_memory -= new_header->size;
    }

    return (void *) ((char *) start_header + sizeof(struct header));
}

void heap_lock(void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));
    SET_KIND(header, KIND_IMMOVABLE);
}

void heap_unlock(void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));
    SET_KIND(header, KIND_MOVABLE);
}

void heap_free(struct heap *heap, void *ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));

    header->flags = KIND_AVAILABLE;
    heap->used_memory -= header->size;

    // check if the block directly after this one is available, and merge them if it is
    struct header *next = header->next;
    if (next != NULL && GET_KIND(next) == KIND_AVAILABLE) {
        header->size += next->size;
        next = next->next;

        if (next->next != NULL) {
            next->next->prev = header;
        }
    }

    // merge with the block directly before if applicable
    struct header *prev = header->prev;
    if (prev != NULL && GET_KIND(prev) == KIND_AVAILABLE) {
        prev->size += header->size;
        prev->next = header->next;

        if (header->next != NULL) {
            header->next->prev = prev;
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
            kind_names[GET_KIND(header)]
        );

        if (header->next != NULL) {
            header = header->next;
        } else {
            break;
        }
    }
}

void heap_set_update_absolute(void *ptr, void **absolute_ptr) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));

    // TODO: this section is critical, should interrupts be disabled?
    header->flags &= ~FLAG_KMEM_UPDATE;
    header->update_ref.absolute_ptr = absolute_ptr;
}

void heap_set_update_handle(void *ptr, kmem_handle_t handle) {
    struct header *header = (struct header *) ((char *) ptr - sizeof(struct header));

    // TODO: this section is critical, should interrupts be disabled?
    header->flags |= FLAG_KMEM_UPDATE;
    header->update_ref.handle = handle;
}
