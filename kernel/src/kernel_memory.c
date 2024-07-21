#include "kernel_memory.h"
#include "debug.h"

struct handle_block {
    void *pointers[32];
    uint32_t used;
    struct handle_block *next;
    struct handle_block *prev;
};

static struct heap *kernel_heap = NULL;
static struct handle_block *handle_root = NULL;

void kmem_init(struct heap *heap) {
    kernel_heap = heap;

    handle_root = heap_alloc(kernel_heap, sizeof(struct handle_block));
    handle_root->used = 0;
    handle_root->next = NULL;
    handle_root->prev = NULL;
    heap_set_update_absolute((void *) handle_root, (void **) &handle_root);
    heap_unlock(handle_root);
}

kmem_handle_t kmem_alloc(size_t size, bool lock_initially, void **locked_address) {
    // TODO: how should all this be handled in order to prevent possible race conditions?

    printk("kmem: allocating size %d, locked initially: %s\n", size, lock_initially ? "yes" : "no");

    heap_lock(handle_root);
    struct handle_block *handle_block = handle_root;
    kmem_handle_t handle_offset = 0;

    while (1) {
        if ((handle_block->used & 0xfffffffe) != 0xfffffffe) {
            int i;
            for (i = 1; i < 32 && (handle_block->used & (1 << i)) != 0; i ++);
            kmem_handle_t handle = handle_offset + i;

            printk("kmem: found available handle %d for allocation\n", handle);

            void *allocation = heap_alloc(kernel_heap, size);
            if (allocation == NULL) {
                heap_unlock(handle_block);
                return 0;
            }

            handle_block->used |= (1 << i);
            handle_block->pointers[i] = allocation;
            heap_set_update_handle((void *) allocation, handle);

            heap_unlock(handle_block);
            if (lock_initially) {
                *locked_address = allocation;
            } else {
                heap_unlock(allocation);
            }

            return handle;
        }

        if (handle_block->next == NULL) {
            // allocate a new handle block and add it to the list
            printk("kmem: allocating new handle block at %d for %d-%d\n", handle_offset, handle_offset + 32, handle_offset + 64);

            struct handle_block *new = heap_alloc(kernel_heap, sizeof(struct handle_block));
            if (new == NULL) {
                heap_unlock(handle_block);
                return 0;
            }

            new->used = 0;
            new->next = NULL;
            new->prev = handle_block;

            handle_block->used |= 1;
            handle_block->pointers[0] = new;
            handle_block->next = new;
            heap_set_update_handle((void *) new, handle_offset);
        } else {
            heap_lock(handle_block->next);
        }

        struct handle_block *next = handle_block->next;
        heap_unlock(handle_block);
        handle_block = next;
        handle_offset += 32;
    }
}

void kmem_update(kmem_handle_t handle, void *new_address) {
    printk("kmem: updating handle %d to 0x%08x\n", handle, new_address);

    if ((handle & 31) == 0) {
        // this handle points to a handle block, no need to search thru everything
        struct handle_block *new_block_address = new_address;

        // TODO: should these be locked? does it matter?
        struct handle_block *prev = new_block_address->prev;
        if (prev != NULL) {
            prev->next = new_block_address;
            prev->pointers[0] = new_address;
        }

        struct handle_block *next = new_block_address->next;
        if (next != NULL) {
            next->prev = new_block_address;
        }
    }

    heap_lock(handle_root);
    struct handle_block *handle_block = handle_root;

    while (1) {
        if (handle < 32) {
            handle_block->pointers[handle] = new_address;
            heap_unlock(handle_block);
            return;
        }

        if (handle_block->next == NULL) {
            heap_unlock(handle_block);
            printk("kmem: handle is too big\n");
            return;
        }

        heap_lock(handle_block->next);
        struct handle_block *next = handle_block->next;
        heap_unlock(handle_block);
        handle_block = next;
        handle -= 32;
    }
}

void kmem_free(kmem_handle_t handle) {
    printk("kmem: freeing handle %d\n", handle);

    heap_lock(handle_root);
    struct handle_block *handle_block = handle_root;

    while (1) {
        if (handle < 32) {
            uint32_t handle_bit = 1 << handle;

            if ((handle_block->used | handle_bit) != 0) {
                heap_free(kernel_heap, handle_block->pointers[handle]);
                handle_block->used &= ~handle_bit;
            }

            // walk backwards and free any empty handle blocks at the end of the list
            struct handle_block *block = handle_block;
            while (block->used == 0 && block->next == NULL) {
                heap_lock(block->prev);
                struct handle_block *prev = block->prev;
                prev->next = NULL;
                prev->used &= ~1;
                heap_free(kernel_heap, (void *) block);
                block = prev;
            }
            heap_unlock(block);

            return;
        }

        if (handle_block->next == NULL) {
            heap_unlock(handle_block);
            printk("kmem: handle is too big\n");
            return;
        }

        heap_lock(handle_block->next);
        struct handle_block *next = handle_block->next;
        heap_unlock(handle_block);
        handle_block = next;
        handle -= 32;
    }
}

void *kmem_lock(kmem_handle_t handle) {
    printk("kmem: locking handle %d\n", handle);

    heap_lock(handle_root);
    struct handle_block *handle_block = handle_root;

    while (1) {
        if (handle < 32) {
            if ((handle_block->used | (1 << handle)) != 0) {
                heap_lock(handle_block->pointers[handle]);
                heap_unlock(handle_block);
                return handle_block->pointers[handle];
            } else {
                heap_unlock(handle_block);
                return NULL;
            }
        }

        if (handle_block->next == NULL) {
            heap_unlock(handle_block);
            printk("kmem: handle is too big\n");
            return NULL;
        }

        heap_lock(handle_block->next);
        struct handle_block *next = handle_block->next;
        heap_unlock(handle_block);
        handle_block = next;
        handle -= 32;
    }
}

void kmem_unlock(kmem_handle_t handle) {
    printk("kmem: unlocking handle %d\n", handle);

    heap_lock(handle_root);
    struct handle_block *handle_block = handle_root;

    while (1) {
        if (handle < 32) {
            if ((handle_block->used | (1 << handle)) != 0) {
                heap_unlock(handle_block->pointers[handle]);
            }

            heap_unlock(handle_block);
            return;
        }

        if (handle_block->next == NULL) {
            heap_unlock(handle_block);
            printk("kmem: handle is too big\n");
            return;
        }

        heap_lock(handle_block->next);
        struct handle_block *next = handle_block->next;
        heap_unlock(handle_block);
        handle_block = next;
        handle -= 32;
    }
}
