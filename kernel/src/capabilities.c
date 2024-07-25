#include "capabilities.h"
#include "string.h"
#include "threads.h"
#include "scheduler.h"

#undef DEBUG_CAPABILITIES

struct capability kernel_root_capability;

static void update_capability_lists(struct capability *capability) {
    // resource lock/unlock is omitted here since no allocations take place

    if (capability->resource_list.prev != NULL) {
        capability->resource_list.prev->resource_list.next = capability;
    }

    if (capability->resource_list.next != NULL) {
        capability->resource_list.next->resource_list.prev = capability;
    }

    // these are afterwards so that any references to this capability in the list are valid
    if (capability->resource_list.prev == NULL) {
        for (struct capability *c = capability; c != NULL; c = c->resource_list.next) {
            c->resource_list.start = capability;
        }
    }

    if (capability->resource_list.next == NULL) {
        for (struct capability *c = capability; c != NULL; c = c->resource_list.next) {
            c->resource_list.end = capability;
        }
    }

    if (capability->derivation_list.prev != NULL) {
        capability->derivation_list.prev->derivation_list.next = capability;
    }

    if (capability->derivation_list.next != NULL) {
        capability->derivation_list.next->derivation_list.prev = capability;
    }

    if (capability->derivation_list.prev == NULL && capability->derived_from != NULL) {
        capability->derived_from->derivation_list_start = capability;
    }

    if (capability->derivation_list.next == NULL && capability->derived_from != NULL) {
        capability->derived_from->derivation_list_end = capability;
    }

    for (struct capability *c = capability->derivation_list_start; c != NULL; c = c->derivation_list.next) {
        c->derived_from = capability;
    }
}

void move_capability(struct capability *from, struct capability *to) {
    memcpy(to, from, sizeof(struct capability));
    update_capability_lists(to);
    from->handlers = NULL;
}

/* ==== capability node ==== */

struct capability_node_header {
    size_t slot_bits;
};

static size_t node_copy(struct capability *slot, size_t argument, bool from_userland) {
    struct node_copy_args *args = (struct node_copy_args *) argument;

    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node_header *header = (struct capability_node_header *) slot->resource;

    // make sure slot id is valid
    if (args->dest_slot >= 1 << header->slot_bits) {
        return 0;
    }

    struct capability *dest = (struct capability *) ((char *) header + sizeof(struct capability_node_header) + args->dest_slot * sizeof(struct capability));

    // make sure destination slot is empty
    // TODO: is this fine? or should the capability in the destination slot be deleted
    if (dest->handlers != NULL) {
        return 0;
    }

    struct look_up_result result;
    if (!look_up_capability_relative(args->source_address, args->source_depth, from_userland, &result)) {
        return 0;
    }

    // if badge setting is requested, make sure the source capability is original
    if (args->should_set_badge && result.slot->derived_from != NULL) {
        unlock_looked_up_capability(&result);
        return 0;
    }

    memcpy(dest, result.slot, sizeof(struct capability));
#ifdef DEBUG_CAPABILITIES
    printk("node_copy: source: 0x%x, dest: 0x%x\n", result.slot, dest);
#endif

    // add the newly copied capability to its resource list
    struct capability *end = result.slot->resource_list.end;

    end->resource_list.next = dest;
    dest->resource_list.prev = end;
    dest->resource_list.next = NULL;

    for (struct capability *c = end->resource_list.start; c != NULL; c = c->resource_list.next) {
        c->resource_list.end = dest;
    }

#ifdef DEBUG_CAPABILITIES
    struct capability *c = end->resource_list.start;
    printk("node_copy: resource start: 0x%x, end: 0x%x\n", c->resource_list.start, c->resource_list.end);
    for (; c != NULL; c = c->resource_list.next) {
        printk(" - 0x%x\n", c);
    }
#endif

    // add the newly copied capability to the derivation list of the capability it was copied from
    if (result.slot->derivation_list_end == NULL) {
        // the list is empty, so initialize it with only the new capability
        result.slot->derivation_list_start = dest;
        result.slot->derivation_list_end = dest;
        dest->derivation_list.prev = NULL;
        dest->derivation_list.next = NULL;
    } else {
        struct capability *derivation_end = result.slot->derivation_list_end;

        derivation_end->derivation_list.next = dest;
        dest->derivation_list.prev = derivation_end;
        dest->derivation_list.next = NULL;

        result.slot->derivation_list_end = dest;
    }

    dest->derived_from = result.slot;
    dest->derivation_list_start = NULL;
    dest->derivation_list_end = NULL;

#ifdef DEBUG_CAPABILITIES
    c = result.slot->derivation_list_start;
    printk("node_copy: derivation start: 0x%x, end: 0x%x\n", c, result.slot->derivation_list_end);
    for (; c != NULL; c = c->derivation_list.next) {
        printk(" - 0x%x\n", c);
    }
#endif

    if (args->should_set_badge) {
        dest->badge = args->badge;
    }

    unlock_looked_up_capability(&result);

    return 1;
}

struct invocation_handlers node_handlers = {
    1,
    {node_copy}
};

/// allocates memory for a capability node and initializes it.
/// the size of this capability node is determined by slot_bits, where 2 raised to the power of slot_bits is the number of slots in this node
/// if successful, a pointer to the allocated memory is returned
static void *allocate_node(struct heap *heap, size_t slot_bits) {
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

bool look_up_capability(struct capability *root, size_t address, size_t depth, struct look_up_result *result) {
    if (root->handlers != &node_handlers) {
        return false;
    }

    address &= ((1 << depth) - 1); // make sure there aren't any invalid bits outside of the address

    bool should_unlock = heap_lock(root->resource);
    struct capability_node_header *header = (struct capability_node_header *) root->resource;

    while (1) {
        size_t index_in_node = address & ((1 << header->slot_bits) - 1);
        struct capability *slot = (struct capability *) ((char *) header + sizeof(struct capability_node_header) + index_in_node * sizeof(struct capability));

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

void unlock_looked_up_capability(struct look_up_result *result) {
    if (result->should_unlock) {
        heap_unlock((void *) result->container);
    }
}

bool look_up_capability_relative(size_t address, size_t depth, bool from_userland, struct look_up_result *result) {
    if (!from_userland) {
        return look_up_capability(&kernel_root_capability, address, depth, result);
    }

    if (scheduler_state.current_thread == NULL) {
        return false;
    }

    bool should_unlock = heap_lock(scheduler_state.current_thread);
    bool return_value = look_up_capability(&scheduler_state.current_thread->root_capability, address, depth, result);

    if (should_unlock) {
        heap_unlock(scheduler_state.current_thread);
    }

    return return_value;
}

bool look_up_capability_absolute(struct absolute_capability_address *address, struct look_up_result *result) {
    if (address->thread_id == 0) {
        return look_up_capability(&kernel_root_capability, address->address, address->depth, result);
    }

    struct thread_capability *thread = NULL;
    bool should_unlock = look_up_thread_by_id(address->thread_id, address->bucket_number, &thread);

    if (thread == NULL) {
        return false;
    }

    bool return_value = look_up_capability(&thread->root_capability, address->address, address->depth, result);

    if (should_unlock) {
        heap_unlock(thread);
    }

    return return_value;
}

/// populates a capability slot at the given address and search depth with the given heap-managed resource and invocation handlers
static bool populate_capability_slot(struct heap *heap, size_t address, size_t depth, bool from_userland, void *resource, struct invocation_handlers *handlers) {
    struct look_up_result result;
    if (!look_up_capability_relative(address, depth, from_userland, &result)) {
        heap_free(heap, resource);
        return false;
    }

    // make sure destination slot is empty
    // TODO: is this fine? or should the capability in the slot be deleted
    if (result.slot->handlers != NULL) {
        heap_free(heap, resource);
        unlock_looked_up_capability(&result);
        return false;
    }

    memset(result.slot, 0, sizeof(struct capability));
    result.slot->handlers = handlers;
    result.slot->resource = resource;
    result.slot->flags = CAP_FLAG_IS_HEAP_MANAGED;
    result.slot->access_rights = -1; // all rights given
    result.slot->resource_list.start = result.slot;
    result.slot->resource_list.end = result.slot;
    // everything else here assumes NULL is 0

    struct absolute_capability_address absolute_address;

    if (from_userland && scheduler_state.current_thread != NULL) {
        bool should_unlock = heap_lock(scheduler_state.current_thread);

        // get thread id and bucket number from current thread
        absolute_address.thread_id = scheduler_state.current_thread->thread_id;
        absolute_address.bucket_number = scheduler_state.current_thread->bucket_number;

        if (should_unlock) {
            heap_unlock(scheduler_state.current_thread);
        }
    } else {
        // probably in the kernel, use the reserved thread id of 0 to indicate that
        absolute_address.thread_id = 0;
    }

    absolute_address.address = address;
    absolute_address.depth = depth;

    heap_set_update_capability(resource, &absolute_address);
    heap_unlock(resource);

    unlock_looked_up_capability(&result);
    return true;
}

static void on_node_moved(struct capability_node_header *header) {
    struct capability *slot = (struct capability *) ((char *) header + sizeof(struct capability_node_header));

    for (int i = 0; i < header->slot_bits; i ++) {
        if (slot->handlers != NULL) {
            update_capability_lists(slot);
        }

        slot ++;
    }
}

/* ==== untyped memory ==== */

// TODO: wrap these in a mutex

static size_t untyped_lock(struct capability *slot, size_t argument, bool from_userland) {
    if (!heap_lock(slot->resource)) {
        printk("untyped_lock: attempted to lock a memory region twice!\n");
    }
    return (size_t) slot->resource;
}

static size_t untyped_unlock(struct capability *slot, size_t argument, bool from_userland) {
    heap_unlock(slot->resource);
    return 0;
}

static size_t untyped_try_lock(struct capability *slot, size_t argument, bool from_userland) {
    if (heap_lock(slot->resource)) {
        return (size_t) slot->resource;
    } else {
        return (size_t) NULL;
    }
}

struct invocation_handlers untyped_handlers = {
    3,
    {untyped_lock, untyped_unlock, untyped_try_lock}
};

/* ==== address space ==== */

struct address_space_capability {
    struct heap *heap_pointer; // pointer to a statically allocated heap object
    // TODO: support address spaces other than the one belonging to the kernel
};

static size_t alloc(struct capability *slot, size_t argument, bool from_userland) {
    struct alloc_args *args = (struct alloc_args *) argument;

    // make sure there's permission to create this kind of object
    if ((slot->access_rights & (1 << args->type)) == 0) {
        return 0;
    }

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
    case TYPE_THREAD:
        alloc_thread(heap, &resource, &handlers);
        break;
    }

    if (resource == NULL) {
        return 0;
    }

    return (size_t) populate_capability_slot(heap, args->address, args->depth, from_userland, resource, handlers);
}

struct invocation_handlers address_space_handlers = {1, {alloc}};

/* ==== misc ==== */

void init_root_capability(struct heap *heap) {
    kernel_root_capability.handlers = &node_handlers;
    kernel_root_capability.resource = allocate_node(heap, ROOT_CAP_SLOT_BITS);
    kernel_root_capability.flags = CAP_FLAG_IS_HEAP_MANAGED;
    heap_set_update_absolute(kernel_root_capability.resource, (void **) &kernel_root_capability.resource);
    heap_unlock(kernel_root_capability.resource);

    struct address_space_capability *address_space_resource = heap_alloc(heap, sizeof(struct address_space_capability));
    address_space_resource->heap_pointer = heap;
    populate_capability_slot(heap, 0, ROOT_CAP_SLOT_BITS, false, (void *) address_space_resource, &address_space_handlers);
}

void update_capability_resource(struct absolute_capability_address *address, void *new_resource_address) {
    struct look_up_result result;

    if (!look_up_capability_absolute(address, &result)) {
        printk("update_capability_resource: couldn't locate capability at 0x%x:0x%x (%d bits)\n", address->thread_id, address->address, address->depth);
        return;
    }

    result.slot->resource = new_resource_address;

    if (result.slot->handlers == &node_handlers) {
        on_node_moved((struct capability_node_header *) result.slot->resource);
    } else if (result.slot->handlers == &thread_handlers) {
        on_thread_moved((struct thread_capability *) result.slot->resource);
    }

    // TODO: update locations of any capabilities derived from this one

    unlock_looked_up_capability(&result);
}

size_t invoke_capability(size_t address, size_t depth, size_t handler_number, size_t argument, bool from_userland) {
    struct look_up_result result;

    if (!look_up_capability_relative(address, depth, from_userland, &result)) {
        printk("invoke_capability: couldn't locate capability at 0x%x (%d bits) for invocation\n", address, depth);
        return 0;
    }

    struct invocation_handlers *handlers = result.slot->handlers;

    if (handlers == NULL || handler_number >= handlers->num_handlers) {
        printk("invoke_capability: invocation %d on capability 0x%x (%d bits) is invalid\n", handler_number, address, depth);
        unlock_looked_up_capability(&result);
        return 0;
    }

    size_t return_value = handlers->handlers[handler_number](result.slot, argument, from_userland);
    unlock_looked_up_capability(&result);
    return return_value;
}
