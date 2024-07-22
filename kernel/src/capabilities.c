#include "capabilities.h"
#include "string.h"

#define ROOT_CAP_SLOT_BITS 4

struct capability kernel_root_capability;

/* ==== capability node ==== */

struct capability_node_header {
    size_t slot_bits;
};

struct invocation_handlers node_handlers = {
    0,
    {}
};

// allocates memory for a capability node and initializes it.
// the size of this capability node is determined by slot_bits, where 2 raised to the power of slot_bits is the number of slots in this node
// if successful, a pointer to the allocated memory is returned
void *allocate_node(struct heap *heap, size_t slot_bits) {
    size_t total_slots = 1 << slot_bits;

    size_t slots_start = sizeof(struct capability_node_header);
    size_t slots_size = total_slots * sizeof(struct capability);

    struct capability_node_header *new = (struct capability_node_header *) heap_alloc(heap, slots_start + slots_size);
    if (new == NULL) {
        return NULL;
    }

    new->slot_bits = slot_bits;

    struct capability *slots = (struct capability *) ((char *) new + slots_start);
    for (int i = 0; i < total_slots; i ++) {
        (slots ++)->handlers = NULL;
    }

    return (void *) new;
}

struct look_up_result {
    struct capability *slot;
    struct capability_node_header *container;
    bool should_unlock;
};

bool look_up_capability(struct capability *root, size_t address, size_t depth, struct look_up_result *result) {
    if (root->handlers != &node_handlers && depth != 0) {
        return false;
    }

    address &= ((1 << depth) - 1); // make sure there aren't any invalid bits outside of the address

    bool should_unlock = heap_lock(root->resource);
    struct capability_node_header *header = (struct capability_node_header *) root->resource;

    while (1) {
        size_t index_in_node = address & ((1 << header->slot_bits) - 1);
        struct capability *slot = (struct capability *) ((char *) header + sizeof(struct capability_node_header) + index_in_node * sizeof(struct capability));

        // TODO: should the capability be checked for validity?

        if (depth == header->slot_bits) {
            // finished the search!
            result->slot = slot;
            result->container = header;
            result->should_unlock = should_unlock;
            return true;
        }

        // sanity check. if the depth value doesn't match how many bits are in the node or if the slot doesn't contain a node, give up
        if (depth < header->slot_bits || slot->handlers != &node_handlers) {
            if (should_unlock) {
                heap_unlock((void *) header);
            }
            return false;
        }

        address >>= header->slot_bits;
        depth -= header->slot_bits;

        bool next_should_unlock = heap_lock(slot->resource);
        struct capability_node_header *next = (struct capability_node_header *) slot->resource;

        if (should_unlock) {
            heap_unlock((void *) header);
        }

        should_unlock = next_should_unlock;
        header = next;
    }
}

// TODO: find a better name for this
void unlock_looked_up_capability(struct look_up_result *result) {
    if (result->should_unlock) {
        heap_unlock((void *) result->container);
    }
}

// populates a capability slot at the given address and search depth with the given heap-managed resource and invocation handlers
bool populate_capability_slot(struct heap *heap, size_t address, size_t depth, void *resource, struct invocation_handlers *handlers) {
    struct look_up_result result;
    if (!look_up_capability(&kernel_root_capability, address, depth, &result)) {
        heap_free(heap, resource);
        return false;
    }

    result.slot->handlers = handlers;
    result.slot->resource = resource;
    result.slot->flags = CAP_FLAG_IS_HEAP_MANAGED;
    result.slot->badge = 0;
    heap_add_reference(resource);
    heap_set_update_capability(resource, address, depth);
    heap_unlock(resource);

    unlock_looked_up_capability(&result);
    return true;
}

/* ==== untyped memory ==== */

// TODO: wrap these in a mutex

static size_t lock(struct capability *slot, size_t argument, bool from_userland) {
    if (!heap_lock(slot->resource)) {
        printk("attempted to lock a memory region twice!\n");
    }
    return (size_t) slot->resource;
}

static size_t unlock(struct capability *slot, size_t argument, bool from_userland) {
    heap_unlock(slot->resource);
    return 0;
}

static size_t try_lock(struct capability *slot, size_t argument, bool from_userland) {
    if (heap_lock(slot->resource)) {
        return (size_t) slot->resource;
    } else {
        return (size_t) NULL;
    }
}

struct invocation_handlers untyped_handlers = {
    3,
    {lock, unlock, try_lock}
};

/* ==== address space ==== */

struct address_space_capability {
    struct heap *heap_pointer; // pointer to a statically allocated heap object
    // TODO: support address spaces other than the one belonging to the kernel
};

static size_t alloc(struct capability *slot, size_t argument, bool from_userland) {
    struct alloc_args *args = (struct alloc_args *) argument;

    // get pointer to heap
    bool should_unlock = heap_lock(slot->resource);
    struct address_space_capability *heap_resource = (struct address_space_capability *) slot->resource;

    struct heap *heap = heap_resource->heap_pointer;

    if (should_unlock) {
        heap_unlock((void *) heap_resource);
    }

    // allocate resource
    void *resource = NULL;
    struct invocation_handlers *handlers;

    switch (args->type) {
    case TYPE_UNTYPED:
        resource = heap_alloc(heap, args->size);
        handlers = &untyped_handlers;
        break;
    case TYPE_NODE:
        resource = allocate_node(heap, args->size);
        handlers = &node_handlers;
        break;
    }

    if (resource == NULL) {
        return 0;
    }

    return (size_t) populate_capability_slot(heap, args->address, args->depth, resource, handlers);
}

struct invocation_handlers address_space_handlers = {
    1,
    {alloc}
};

/* ==== misc ==== */

void init_root_capability(struct heap *heap) {
    kernel_root_capability.handlers = &node_handlers;
    kernel_root_capability.resource = allocate_node(heap, ROOT_CAP_SLOT_BITS);
    kernel_root_capability.flags = CAP_FLAG_IS_HEAP_MANAGED;
    heap_set_update_absolute(kernel_root_capability.resource, (void **) &kernel_root_capability.resource);
    heap_unlock(kernel_root_capability.resource);

    struct address_space_capability *address_space_resource = heap_alloc(heap, sizeof(struct address_space_capability));
    address_space_resource->heap_pointer = heap;
    populate_capability_slot(heap, 0, ROOT_CAP_SLOT_BITS, (void *) address_space_resource, &address_space_handlers);
}

void update_capability_resource(size_t address, size_t depth, void *new_resource_address) {
    struct look_up_result result;

    if (!look_up_capability(&kernel_root_capability, address, depth, &result)) {
        printk("couldn't locate capability at 0x%x (%d bits) for resource move\n", address, depth);
        return;
    }

    result.slot->resource = new_resource_address;
    // TODO: update locations of any capabilities derived from this one

    unlock_looked_up_capability(&result);
}
