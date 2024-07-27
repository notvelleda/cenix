#include "capabilities.h"
#include "ipc.h"
#include "linked_list.h"
#include "scheduler.h"
#include "string.h"
#include "threads.h"

#undef DEBUG_CAPABILITIES

struct capability kernel_root_capability;

void update_capability_references(struct capability *capability) {
    LIST_UPDATE_ADDRESS_NO_CONTAINER(struct capability, resource_list, capability);
    LIST_UPDATE_ADDRESS_NO_CONTAINER(struct capability, derivation_list, capability);

    // if this capability has other capabilities derived from it, update their `derived_from` references to point to the new address
    if ((capability->flags & (CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED)) != 0 && capability->derivation != NULL) {
        LIST_ITER_NO_CONTAINER(struct capability, derivation_list, capability->derivation, link) {
            link->derived_from = capability;
        }
    }

    // if this is the first item in the derivation list that this capability is in and the capability it was derived from still exists,
    // update its parent capability to point to this one
    if (capability->derivation_list.prev == NULL && capability->derived_from != NULL) {
        capability->derived_from->derivation = capability;
    }
}

void move_capability(struct capability *from, struct capability *to) {
    memcpy(to, from, sizeof(struct capability));
    update_capability_references(to);
    from->handlers = NULL;
}

static void merge_derivation_lists(struct capability *to_merge) {
    // if this capability was badged and has a child derivation list, insert it into the derivation list of its parent if it still exists
    if ((to_merge->flags & CAP_FLAG_BADGED) != 0 && to_merge->derived_from != NULL && to_merge->derivation != NULL) {
        // update the derived_from pointers for every item in the list
        LIST_ITER_NO_CONTAINER(struct capability, derivation_list, to_merge->derivation, link) {
            link->derived_from = to_merge->derived_from;
        }

        if (to_merge->derivation_list.start == to_merge->derivation_list.end) {
            // there's only one item in the list (presumably this one) so the list doesn't need to be merged
            to_merge->derived_from->derivation = to_merge->derivation;
        } else {
            // there's multiple items in the list, insert the child list right before the last item in the parent list
            struct capability *derivation = to_merge->derivation;

            struct capability *prev = to_merge->derivation_list.end->derivation_list.prev;
            prev->derivation_list.next = derivation->derivation_list.start;
            derivation->derivation_list.start->derivation_list.prev = prev;

            to_merge->derivation_list.end->derivation_list.prev = derivation->derivation_list.end;
            derivation->derivation_list.end->derivation_list.next = derivation->derivation_list.end;

            LIST_ITER_NO_CONTAINER(struct capability, derivation_list, to_merge->derivation, link) {
                link->derivation_list.start = to_merge->derivation_list.start;
                link->derivation_list.end = to_merge->derivation_list.end;
            }
        }
    }

    // if this is the first item in the derivation list and the capability that this was derived from still exists,
    // update the original capability's derivation pointer to the next element in the list
    if (to_merge->derivation_list.prev == NULL && to_merge->derived_from != NULL) {
        to_merge->derived_from->derivation = to_merge->derivation_list.next;
    }

    LIST_DELETE_NO_CONTAINER(struct capability, derivation_list, to_merge);
}

void delete_capability(struct capability *to_delete) {
    bool destroy_resource = false;

    // check if this resource is heap managed and the capability to be deleted is at the start of the resource list
    if (to_delete->resource_list.prev == NULL) {
        if (to_delete->resource_list.next == NULL) {
            // this is the last capability in the list, free up its resource
            destroy_resource = true;
        } else {
            // move ownership of this resource to the next item in the list
            heap_set_update_capability(to_delete->resource, &to_delete->resource_list.next->address);
        }
    }

    LIST_DELETE_NO_CONTAINER(struct capability, resource_list, to_delete);

    if (destroy_resource) {
        if (to_delete->handlers->destructor != NULL) {
            to_delete->handlers->destructor(to_delete);
        }

        heap_free(to_delete->heap, to_delete->resource);
    }

    to_delete->handlers = NULL;
}

#ifdef DEBUG
void print_capability_lists(struct capability *capability) {
    struct capability *c = capability->resource_list.start;
    printk("resource start: 0x%x, end: 0x%x\n", c, capability->resource_list.end);
    for (; c != NULL; c = c->resource_list.next) {
        printk(" - 0x%x\n", c);
    }
    c = capability->derivation_list.start;
    printk("derivation start: 0x%x, end: 0x%x\n", c, capability->derivation_list.end);
    for (; c != NULL; c = c->derivation_list.next) {
        printk(" - 0x%x\n", c);
    }
}
#endif

void update_capability_addresses(struct capability *slot, const struct absolute_capability_address *address, uint8_t nesting) {
    if (slot->resource_list.prev == NULL) {
        // only update the owner id of the node if it's the owner of the resource
#ifdef DEBUG_CAPABILITIES
        printk(
            "update_capability_addresses: switching owner of 0x%x from 0x%x:0x%x (%d bits) to 0x%x:0x%x (%d bits)\n",
            slot->resource,
            slot->address.thread_id,
            slot->address.address,
            slot->address.depth,
            address->thread_id,
            address->address,
            address->depth
        );
#endif
        heap_set_update_capability(slot->resource, address);
    }

    // update the address of this capability
    slot->address = *address;

    if (slot->handlers == &node_handlers) {
        struct capability_node_header *header = (struct capability_node_header *) slot->resource;

        header->nested_nodes = nesting;

        struct capability *slot_in_node = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header));
        for (int i = 0; i < (1 << header->slot_bits); i ++, slot_in_node ++) {
            if (slot_in_node->handlers == NULL) {
                continue;
            }

            struct absolute_capability_address new_address;
            new_address.address = (i << address->depth) | address->address;
            new_address.depth = address->depth + header->slot_bits;
            new_address.thread_id = address->thread_id;
            new_address.bucket_number = address->bucket_number;

            update_capability_addresses(slot_in_node, &new_address, nesting + 1);
        }
    }
}

/* ==== capability node ==== */

static size_t node_copy(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    const struct node_copy_args *args = (struct node_copy_args *) argument;

    // resource lock/unlock is omitted here since no allocations take place
    const struct capability_node_header *header = (struct capability_node_header *) slot->resource;

    // make sure slot id is valid
    if (args->dest_slot >= 1 << header->slot_bits) {
        return 0;
    }

    struct capability *dest = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header) + args->dest_slot * sizeof(struct capability));

    // make sure destination slot is empty
    if (dest->handlers != NULL) {
        return 0;
    }

    struct look_up_result result;
    if (!look_up_capability_relative(args->source_address, args->source_depth, from_userland, &result)) {
        return 0;
    }

    // make sure the source capability is eligible for copying.
    // copying of capability nodes isn't allowed because i just don't know how to deal with it currently and it may just overcomplicate things
    if (result.slot->handlers == &node_handlers) {
        unlock_looked_up_capability(&result);
        return 0;
    }

    // if badge setting is requested, make sure the source capability is original
    if (args->should_set_badge && (result.slot->flags & CAP_FLAG_ORIGINAL) == 0) {
        unlock_looked_up_capability(&result);
        return 0;
    }

    memcpy(dest, result.slot, sizeof(struct capability));
#ifdef DEBUG_CAPABILITIES
    printk("node_copy: source: 0x%x, dest: 0x%x\n", result.slot, dest);
#endif

    dest->flags &= ~(CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED);

    dest->address.address = address | (args->dest_slot << depth);
    dest->address.depth = depth + header->slot_bits;

    // add the newly copied capability to the resource list of the one it was copied from
    LIST_INSERT_NO_CONTAINER(struct capability, result.slot, resource_list, dest);

#ifdef DEBUG_CAPABILITIES
    printk("node_copy: source derivation: 0x%x, derived_from: 0x%x\n", result.slot->derivation, result.slot->derived_from);
#endif

    if ((result.slot->flags & (CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED)) != 0) {
        dest->derived_from = result.slot;

        if (result.slot->derivation == NULL) {
            // this new derivation is the first element in the list
            result.slot->derivation = dest;

            LIST_INIT_NO_CONTAINER(dest, derivation_list);
        } else {
            // add this new derivation to the derivation list originating from the source capability
            LIST_INSERT_NO_CONTAINER(struct capability, result.slot->derivation, derivation_list, dest);
        }
    } else {
        // this capability isn't an original derivation, just add it to the derivation list of the source
        LIST_INSERT_NO_CONTAINER(struct capability, result.slot, derivation_list, dest);
    }

    dest->derivation = NULL;

    if (args->should_set_badge) {
        dest->badge = args->badge;
        dest->flags |= CAP_FLAG_BADGED;
    }

#ifdef DEBUG_CAPABILITIES
    printk("src:\n");
    print_capability_lists(result.slot);
    printk("dest:\n");
    print_capability_lists(dest);
#endif

    unlock_looked_up_capability(&result);

    return 1;
}

static size_t node_move(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    const struct node_copy_args *args = (struct node_copy_args *) argument;

    // resource lock/unlock is omitted here since no allocations take place
    const struct capability_node_header *header = (struct capability_node_header *) slot->resource;

    // make sure slot id is valid
    if (args->dest_slot >= 1 << header->slot_bits) {
        return 0;
    }

    struct capability *dest = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header) + args->dest_slot * sizeof(struct capability));

    // make sure destination slot is empty
    if (dest->handlers != NULL) {
        return 0;
    }

    struct look_up_result result;
    if (!look_up_capability_relative(args->source_address, args->source_depth, from_userland, &result)) {
        return 0;
    }

    move_capability(result.slot, dest);

    // update the address of the capability to its new slot
    dest->address.address = address | (args->dest_slot << depth);
    dest->address.depth = depth + header->slot_bits;

    if ((dest->flags & CAP_FLAG_IS_HEAP_MANAGED) != 0 && dest->resource_list.prev == NULL) {
        // only update the owner id of the node if it's the owner of the resource
        heap_set_update_capability(dest->resource, &dest->address);
    }

    unlock_looked_up_capability(&result);

    return 1;
}

static size_t node_delete(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node_header *header = (struct capability_node_header *) slot->resource;

    // make sure slot id is valid
    if (argument >= 1 << header->slot_bits) {
        return 0;
    }

    struct capability *to_delete = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header) + argument * sizeof(struct capability));

    // make sure there's actually a capability here
    if (to_delete->handlers == NULL) {
        return 0;
    }

    merge_derivation_lists(to_delete);
    delete_capability(to_delete);

    return 1;
}

static size_t node_revoke(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node_header *header = (struct capability_node_header *) slot->resource;

    // make sure slot id is valid
    if (argument >= 1 << header->slot_bits) {
        return 0;
    }

    struct capability *to_revoke = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header) + argument * sizeof(struct capability));

    // make sure there's actually a capability here and that this capability is eligible for revocation
    if (
        to_revoke->handlers == NULL
        || to_revoke->handlers == &node_handlers
        || (to_revoke->flags & (CAP_FLAG_BADGED | CAP_FLAG_ORIGINAL)) == 0
    ) {
        return 0;
    }

    // delete all the capabilities that have been derived from this one.
    // derivation lists don't have to be merged here since they'll all be deleted anyways
    if (to_revoke->derivation != NULL) {
        LIST_ITER_NO_CONTAINER(struct capability, derivation_list, to_revoke->derivation, c) {
            if ((c->flags & (CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED)) != 0 && c->derivation != NULL) {
                // this capability has a second derivation tree level, walk it and delete those capabilities too
                LIST_ITER_NO_CONTAINER(struct capability, derivation_list, c->derivation, d) {
                    delete_capability(d);
                }
            }

            delete_capability(c);
        }
    }

    to_revoke->derivation = NULL;

    return 1;
}

static void on_node_moved(void *resource) {
    struct capability_node_header *header = (struct capability_node_header *) resource;
    struct capability *slot = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header));

    for (int i = 0; i < (1 << header->slot_bits); i ++, slot ++) {
        if (slot->handlers != NULL) {
            update_capability_references(slot);
        }
    }
}

static void node_destructor(struct capability *node) {
    const struct capability_node_header *header = (struct capability_node_header *) node->resource;

    struct capability *slot = (struct capability *) ((uint8_t *) node + sizeof(struct capability_node_header));
    for (int i = 0; i < (1 << header->slot_bits); i ++, slot ++) {
        if (slot->handlers != NULL) {
            merge_derivation_lists(slot);
            delete_capability(slot);
        }
    }
}

struct invocation_handlers node_handlers = {
    .num_handlers = 4,
    .handlers = {node_copy, node_move, node_delete, node_revoke},
    .on_moved = on_node_moved,
    .destructor = node_destructor
};

/// \brief allocates memory for a capability node and initializes it
///
/// the size of this capability node is determined by slot_bits, where 2 raised to the power of slot_bits is the number of slots in this node.s
/// if successful, a pointer to the allocated memory is returned
static void *alloc_node(struct heap *heap, size_t slot_bits) {
    size_t total_slots = 1 << slot_bits;

    size_t slots_start = sizeof(struct capability_node_header);
    size_t slots_size = total_slots * sizeof(struct capability);

    struct capability_node_header *new = (struct capability_node_header *) heap_alloc(heap, slots_start + slots_size);
    if (new == NULL) {
        return NULL;
    }

    new->slot_bits = slot_bits;
    new->nested_nodes = 1; // to be filled out in populate_capability_slot() if this isn't the kernel root node

    struct capability *slots = (struct capability *) ((uint8_t *) new + slots_start);
    for (int i = 0; i < total_slots; i ++) {
        (slots ++)->handlers = NULL;
    }

    return new;
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
        struct capability *slot = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header) + index_in_node * sizeof(struct capability));

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
                heap_unlock(header);
            }
            return false;
        }

        address >>= header->slot_bits;
        depth -= header->slot_bits;

        bool next_should_unlock = heap_lock(slot->resource);
        struct capability_node_header *next = (struct capability_node_header *) slot->resource;

        if (should_unlock) {
            heap_unlock(header);
        }

        should_unlock = next_should_unlock;
        header = next;
    }
}

void unlock_looked_up_capability(struct look_up_result *result) {
    if (result->should_unlock) {
        heap_unlock(result->container);
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

bool look_up_capability_absolute(const struct absolute_capability_address *address, struct look_up_result *result) {
    if (address->thread_id == 0) {
        return look_up_capability(&kernel_root_capability, address->address, address->depth, result);
    }

    struct thread_capability *thread = NULL;
    bool should_unlock = look_up_thread_by_id(address->thread_id, address->bucket_number, &thread);

    if (thread == NULL) {
        printk("look_up_capability_absolute: couldn't find thread 0x%x in bucket %d\n", address->thread_id, address->bucket_number);
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
        printk("populate_capability_slot: failed to look up slot at 0x%x (%d bits)\n", address, depth);
        heap_free(heap, resource);
        return false;
    }

    // make sure destination slot is empty
    if (result.slot->handlers != NULL) {
        printk("populate_capability_slot: slot at 0x%x (%d bits) isn't empty\n", address, depth);
        heap_free(heap, resource);
        unlock_looked_up_capability(&result);
        return false;
    }

    if (handlers == &node_handlers && result.container != NULL) {
        const struct capability_node_header *container = (struct capability_node_header *) result.container;

        // make sure this new node isn't too many layers of nesting deep
        if (container->nested_nodes >= MAX_NESTED_NODES) {
            printk("populate_capability_slot: too many levels of nesting\n");
            heap_free(heap, resource);
            unlock_looked_up_capability(&result);
            return false;
        }

        // update the nested nodes count of this new node
        struct capability_node_header *header = (struct capability_node_header *) resource;
        header->nested_nodes = container->nested_nodes + 1;
    }

    memset(result.slot, 0, sizeof(struct capability));
    result.slot->handlers = handlers;
    result.slot->resource = resource;
    result.slot->flags = CAP_FLAG_IS_HEAP_MANAGED | CAP_FLAG_ORIGINAL;
    result.slot->access_rights = -1; // all rights given
    LIST_INIT_NO_CONTAINER(result.slot, resource_list);
    // everything else here assumes NULL is 0

    if (from_userland && scheduler_state.current_thread != NULL) {
        // get thread id and bucket number from current thread
        result.slot->address.thread_id = scheduler_state.current_thread->thread_id;
        result.slot->address.bucket_number = scheduler_state.current_thread->bucket_number;
    } else {
        // probably in the kernel, use the reserved thread id of 0 to indicate that
        result.slot->address.thread_id = 0;
    }

    result.slot->address.address = address;
    result.slot->address.depth = depth;
    result.slot->heap = heap;

    heap_set_update_capability(resource, &result.slot->address);
    heap_unlock(resource);

    unlock_looked_up_capability(&result);
    return true;
}

static uint8_t nesting_depth_search(const struct capability *node) {
    const struct capability_node_header *header = (struct capability_node_header *) node->resource;
    uint8_t nesting_value = header->nested_nodes;

    const struct capability *slot = (struct capability *) ((uint8_t *) header + sizeof(struct capability_node_header));
    for (int i = 0; i < (1 << header->slot_bits); i ++, slot ++) {
        if (slot->handlers != &node_handlers) {
            continue;
        }

        uint8_t new_nesting = nesting_depth_search(slot);

        if (new_nesting > nesting_value) {
            nesting_value = new_nesting;
        }
    }

    return nesting_value;
}

uint8_t get_nested_nodes_depth(const struct capability *node) {
    if (node->handlers != &node_handlers) {
        return 0;
    }

    const struct capability_node_header *header = (struct capability_node_header *) node->resource;
    return nesting_depth_search(node) - header->nested_nodes;
}

/* ==== untyped memory ==== */

// TODO: wrap these in a mutex

static size_t untyped_lock(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    if (!heap_lock(slot->resource)) {
        printk("untyped_lock: attempted to lock a memory region twice!\n");
    }
    return (size_t) slot->resource;
}

static size_t untyped_unlock(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    heap_unlock(slot->resource);
    return 0;
}

static size_t untyped_try_lock(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    if (heap_lock(slot->resource)) {
        return (size_t) slot->resource;
    } else {
        return (size_t) NULL;
    }
}

struct invocation_handlers untyped_handlers = {
    .num_handlers = 3,
    .handlers = {untyped_lock, untyped_unlock, untyped_try_lock},
};

/* ==== address space ==== */

struct address_space_capability {
    struct heap *heap_pointer; // pointer to a statically allocated heap object
    // TODO: support address spaces other than the one belonging to the kernel
};

#ifdef DEBUG
const char *type_names[4] = {"untyped", "node", "thread", "endpoint"};
#endif

static size_t address_space_alloc(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    const struct alloc_args *args = (struct alloc_args *) argument;

    if (args->type >= 4) {
        printk("address_space_alloc: invalid type %d\n", args->type);
        return 0;
    }

    // make sure there's permission to create this kind of object
    if ((slot->access_rights & (1 << args->type)) == 0) {
        printk(
            "address_space_alloc: capability at 0x%x (depth %d) doesn't have permission to create type %d (%s)\n",
            address,
            depth,
            args->type,
            type_names[args->type]
        );
        return 0;
    }

    // get pointer to heap
    //bool should_unlock = heap_lock(slot->resource);
    struct address_space_capability *heap_resource = (struct address_space_capability *) slot->resource;

    struct heap *heap = heap_resource->heap_pointer;

    /*if (should_unlock) {
        heap_unlock(heap_resource);
    }*/

    // allocate resource
    void *resource = NULL;
    struct invocation_handlers *handlers;

    switch (args->type) {
    case TYPE_UNTYPED:
        resource = heap_alloc(heap, args->size);
        handlers = &untyped_handlers;
        break;
    case TYPE_NODE:
        resource = alloc_node(heap, args->size);
        handlers = &node_handlers;
        break;
    case TYPE_THREAD:
        resource = alloc_thread(heap);
        handlers = &thread_handlers;
        break;
    case TYPE_ENDPOINT:
        resource = alloc_endpoint(heap);
        handlers = &endpoint_handlers;
        break;
    }

    if (resource == NULL) {
        printk("address_space_alloc: heap allocation for type %d (%s) with size %d failed\n", args->type, type_names[args->type], args->size);
        return 0;
    }

#ifdef DEBUG_CAPABILITIES
    printk(
        "address_space_alloc: allocated resource 0x%x of type %d (%s) with size %d, handlers 0x%x\n",
        resource,
        args->type,
        type_names[args->type],
        args->size,
        handlers
    );
#endif

    return (size_t) populate_capability_slot(heap, args->address, args->depth, from_userland, resource, handlers);
}

struct invocation_handlers address_space_handlers = {
    .num_handlers = 1,
    .handlers = {address_space_alloc}
};

/* ==== misc ==== */

void init_root_capability(struct heap *heap) {
    memset(&kernel_root_capability, 0, sizeof(struct capability));
    kernel_root_capability.handlers = &node_handlers;
    kernel_root_capability.resource = alloc_node(heap, ROOT_CAP_SLOT_BITS);
    kernel_root_capability.flags = CAP_FLAG_IS_HEAP_MANAGED | CAP_FLAG_ORIGINAL;
    kernel_root_capability.access_rights = -1; // all rights given
    LIST_INIT_NO_CONTAINER(&kernel_root_capability, resource_list);
    kernel_root_capability.heap = heap;
    // everything else here assumes NULL is 0

    heap_set_update_capability(kernel_root_capability.resource, &kernel_root_capability.address);
    heap_unlock(kernel_root_capability.resource);

    struct address_space_capability *address_space_resource = heap_alloc(heap, sizeof(struct address_space_capability));
    address_space_resource->heap_pointer = heap;
    populate_capability_slot(heap, 0, ROOT_CAP_SLOT_BITS, false, address_space_resource, &address_space_handlers);
}

void update_capability_resource(const struct absolute_capability_address *address, void *new_resource_address) {
    struct look_up_result result;

    if (address->thread_id == 0 && address->depth == 0) {
        // update the kernel root capability
        result.slot = &kernel_root_capability;
        result.should_unlock = false;
    } else if (!look_up_capability_absolute(address, &result)) {
        printk("update_capability_resource: couldn't locate capability at 0x%x:0x%x (%d bits)\n", address->thread_id, address->address, address->depth);
        return;
    }

    if (result.slot->handlers == NULL) {
        printk("update_capability_resource: attempted to update resource of invalid capability\n");
        unlock_looked_up_capability(&result);
        return;
    }

    LIST_ITER_NO_CONTAINER(struct capability, resource_list, result.slot, item) {
        item->resource = new_resource_address;
    }

    if (result.slot->handlers->on_moved != NULL) {
        result.slot->handlers->on_moved(result.slot->resource);
    }

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

#ifdef DEBUG_CAPABILITIES
    printk("invoke_capability: invoking %d on 0x%d (%d bits) with argument 0x%x\n", handler_number, address, depth, argument);
#endif

    size_t return_value = handlers->handlers[handler_number](address, depth, result.slot, argument, from_userland);
    unlock_looked_up_capability(&result);

#ifdef DEBUG_CAPABILITIES
    printk("invoke_capability: invocation returned 0x%x\n", return_value);
#endif

    return return_value;
}
