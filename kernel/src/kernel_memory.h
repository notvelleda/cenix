#pragma once

#include <stdint.h>

// this is used in heap.h so must be declared before it's included
typedef uint32_t kmem_handle_t;

#include "heap.h"
#include <stdbool.h>

// initializes the kernel memory handle manager.
// this function keeps a reference to the given heap, so care must be taken that this pointer is valid for
// the entire lifetime of any objects allocated in it via this api
void kmem_init(struct heap *heap);

// allocates memory of the given size and returns a handle to it.
// if lock_initially is set to true, that memory handle will be locked and locked_address will be set to its address.
// if lock_initially is set to false, that memory handle will not be locked
kmem_handle_t kmem_alloc(size_t size, bool lock_initially, void **locked_address);

// updates the given memory handle with a new address
void kmem_update(kmem_handle_t handle, void *new_address);

// frees the memory associated with a given memory handle and frees its handle number up for reuse
void kmem_free(kmem_handle_t handle);

// locks the memory associated with a given handle, returning a pointer to its contents that's valid until that handle is unlocked
void *kmem_lock(kmem_handle_t handle);

// unlocks the memory associated with a given handle, causing any pointers to it to be invalidated
void kmem_unlock(kmem_handle_t handle);
