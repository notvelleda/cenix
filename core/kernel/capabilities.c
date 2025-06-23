#include "capabilities.h"
#include "debug.h"
#include "errno.h"
#include "heap.h"
#include "inttypes.h"
#include "ipc.h"
#include "linked_list.h"
#include "scheduler.h"
#include "string.h"
#include "threads.h"

#undef DEBUG_CAPABILITIES

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

        if (to_delete->resource != NULL) {
            heap_free(to_delete->heap, to_delete->resource);
        }
    }

    to_delete->handlers = NULL;
}

#ifdef DEBUG
void print_capability_lists(struct capability *capability) {
    printk("this capability: 0x%" PRIxPTR "\n", (size_t) capability);
    struct capability *c = capability->resource_list.start;
    printk("resource start: 0x%" PRIxPTR ", end: 0x%" PRIxPTR "\n", (size_t) c, (size_t) capability->resource_list.end);
    for (; c != NULL; c = c->resource_list.next) {
        printk(" - 0x%" PRIxPTR " (start 0x%" PRIxPTR ", end 0x%" PRIxPTR ")\n", (size_t) c, (size_t) c->resource_list.start, (size_t) c->resource_list.end);
    }
    c = capability->derivation_list.start;
    printk("derivation start: 0x%" PRIxPTR ", end: 0x%" PRIxPTR "\n", (size_t) c, (size_t) capability->derivation_list.end);
    for (; c != NULL; c = c->derivation_list.next) {
        printk(" - 0x%" PRIxPTR " (start 0x%" PRIxPTR ", end 0x%" PRIxPTR ")\n", (size_t) c, (size_t) c->derivation_list.start, (size_t) c->derivation_list.end);
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
        struct capability_node *node = (struct capability_node *) slot->resource;

        node->nested_nodes = nesting;

        struct capability *slot_in_node = &node->capabilities[0];
        for (size_t i = 0; i < ((size_t) 1 << node->slot_bits); i ++, slot_in_node ++) {
            if (slot_in_node->handlers == NULL) {
                continue;
            }

            struct absolute_capability_address new_address;
            new_address.address = (i << address->depth) | address->address;
            new_address.depth = address->depth + node->slot_bits;
            new_address.thread_id = address->thread_id;
            new_address.bucket_number = address->bucket_number;

            update_capability_addresses(slot_in_node, &new_address, nesting + 1);
        }
    }
}

void copy_capability(struct capability *source, struct capability *dest, size_t address, size_t depth) {
    memcpy(dest, source, sizeof(struct capability));
#ifdef DEBUG_CAPABILITIES
    printk("copy_capability: source: 0x%x, dest: 0x%x\n", source, dest);
#endif

    dest->flags &= (uint8_t) ~(CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED);

    dest->address.address = address;
    dest->address.depth = depth;

    // add the newly copied capability to the resource list of the one it was copied from
    LIST_INSERT_NO_CONTAINER(struct capability, source, resource_list, dest);

#ifdef DEBUG_CAPABILITIES
    printk("copy_capability: source derivation: 0x%x, derived_from: 0x%x\n", source->derivation, source->derived_from);
#endif

    if ((source->flags & (CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED)) != 0) {
        dest->derived_from = source;

        if (source->derivation == NULL) {
            // this new derivation is the first element in the list
            source->derivation = dest;

            LIST_INIT_NO_CONTAINER(dest, derivation_list);
        } else {
            // add this new derivation to the derivation list originating from the source capability
            LIST_INSERT_NO_CONTAINER(struct capability, source->derivation, derivation_list, dest);
        }
    } else {
        // this capability isn't an original derivation, just add it to the derivation list of the source
        LIST_INSERT_NO_CONTAINER(struct capability, source, derivation_list, dest);
    }

    dest->derivation = NULL;

#ifdef DEBUG_CAPABILITIES
    printk("source:\n");
    print_capability_lists(source);
    printk("dest:\n");
    print_capability_lists(dest);
#endif
}

static const char *handlers_to_name(const struct invocation_handlers *handlers) {
    if (handlers == &debug_handlers) {
        return "debug";
    } else if (handlers == &endpoint_handlers) {
        return "endpoint";
    } else if (handlers == &thread_handlers) {
        return "thread";
    } else if (handlers == &node_handlers) {
        return "node";
    } else if (handlers == &untyped_handlers) {
        return "untyped";
    } else if (handlers == &address_space_handlers) {
        return "address space";
    } else if (handlers == NULL) {
        return "nothing";
    } else {
        return "unknown";
    }
}

/* ==== capability node ==== */

static size_t node_copy(size_t address, size_t depth, struct capability *slot, size_t argument) {
    const struct node_copy_args *args = (struct node_copy_args *) argument;

    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node *node = (struct capability_node *) slot->resource;

    // make sure slot id is valid
    if (args->dest_slot >= (size_t) 1 << node->slot_bits) {
        return EINVAL;
    }

    struct capability *dest = &node->capabilities[args->dest_slot];

    // make sure destination slot is empty
    if (dest->handlers != NULL) {
        printk("node_copy: destination slot is occupied (contains %s)\n", handlers_to_name(dest->handlers));
        return ECAPEXISTS;
    }

    struct look_up_result result;
    if (!look_up_capability_relative(args->source_address, args->source_depth, &result)) {
        printk("node_copy: failed to look up capability to be copied\n");
        return ENOCAPABILITY;
    }

    // make sure the source capability is eligible for copying.
    // copying of capability nodes isn't allowed because i just don't know how to deal with it currently and it may just overcomplicate things
    if (result.slot->handlers == &node_handlers) {
        printk("node_copy: capability nodes are not able to be copied\n");
        unlock_looked_up_capability(&result);
        return ECAPINVAL;
    }

    // if badge setting is requested, make sure the source capability is original
    if (args->should_set_badge && (result.slot->flags & CAP_FLAG_ORIGINAL) == 0) {
        printk("node_copy: source capability is not original\n");
        unlock_looked_up_capability(&result);
        return ECAPINVAL;
    }

    copy_capability(result.slot, dest, address | (args->dest_slot << depth), depth + node->slot_bits);

    if (args->should_set_badge) {
        dest->badge = args->badge;
        dest->flags |= CAP_FLAG_BADGED;
    }

    unlock_looked_up_capability(&result);

    return 0;
}

static size_t node_move(size_t address, size_t depth, struct capability *slot, size_t argument) {
    const struct node_copy_args *args = (struct node_copy_args *) argument;

    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node *node = (struct capability_node *) slot->resource;

    // make sure slot id is valid
    if (args->dest_slot >= (size_t) 1 << node->slot_bits) {
        return EINVAL;
    }

    struct capability *dest = &node->capabilities[args->dest_slot];

    // make sure destination slot is empty
    if (dest->handlers != NULL) {
        return ECAPEXISTS;
    }

    struct look_up_result result;
    if (!look_up_capability_relative(args->source_address, args->source_depth, &result)) {
        return ENOCAPABILITY;
    }

    move_capability(result.slot, dest);

    // update the address of the capability to its new slot
    dest->address.address = address | (args->dest_slot << depth);
    dest->address.depth = depth + node->slot_bits;

    if ((dest->flags & CAP_FLAG_IS_HEAP_MANAGED) != 0 && dest->resource_list.prev == NULL) {
        // only update the owner id of the node if it's the owner of the resource
        heap_set_update_capability(dest->resource, &dest->address);
    }

    unlock_looked_up_capability(&result);

    return 0;
}

static size_t node_delete(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;

    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node *node = (struct capability_node *) slot->resource;

    // make sure slot id is valid
    if (argument >= (size_t) 1 << node->slot_bits) {
        return EINVAL;
    }

    struct capability *to_delete = &node->capabilities[argument];

    // make sure there's actually a capability here
    if (to_delete->handlers == NULL) {
        return ENOCAPABILITY;
    }

    merge_derivation_lists(to_delete);
    delete_capability(to_delete);

    return 0;
}

static size_t node_revoke(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;

    // resource lock/unlock is omitted here since no allocations take place
    struct capability_node *node = (struct capability_node *) slot->resource;

    // make sure slot id is valid
    if (argument >= (size_t) 1 << node->slot_bits) {
        return EINVAL;
    }

    struct capability *to_revoke = &node->capabilities[argument];

    // make sure there's actually a capability here and that this capability is eligible for revocation
    if (to_revoke->handlers == NULL) {
        return ENOCAPABILITY;
    }

    if (to_revoke->handlers == &node_handlers || (to_revoke->flags & (CAP_FLAG_BADGED | CAP_FLAG_ORIGINAL)) == 0) {
        return ECAPINVAL;
    }

    if (to_revoke->derivation == NULL) {
        // no capabilities have been derived from this capability, so nothing needs to be done here :3
        return 0;
    }

    // delete all the capabilities that have been derived from this one.
    // derivation lists don't have to be merged here since they'll all be deleted anyways
    LIST_ITER_NO_CONTAINER(struct capability, derivation_list, to_revoke->derivation, c) {
        if ((c->flags & (CAP_FLAG_ORIGINAL | CAP_FLAG_BADGED)) != 0 && c->derivation != NULL) {
            // this capability has a second derivation tree level, walk it and delete those capabilities too
            LIST_ITER_NO_CONTAINER(struct capability, derivation_list, c->derivation, d) {
                delete_capability(d);
            }
        }

        delete_capability(c);
    }

    to_revoke->derivation = NULL;

    return 0;
}

static void on_node_moved(void *resource) {
    struct capability_node *node = (struct capability_node *) resource;
    struct capability *capability = &node->capabilities[0];

    for (size_t i = 0; i < ((size_t) 1 << node->slot_bits); i ++, capability ++) {
        if (capability->handlers != NULL) {
            update_capability_references(capability);
        }
    }
}

static void node_destructor(struct capability *node_capability) {
    struct capability_node *node = (struct capability_node *) node_capability->resource;
    struct capability *capability = &node->capabilities[0];

    for (size_t i = 0; i < ((size_t) 1 << node->slot_bits); i ++, capability ++) {
        if (capability->handlers != NULL) {
            merge_derivation_lists(capability);
            delete_capability(capability);
        }
    }
}

struct invocation_handlers node_handlers = {
    .num_handlers = 4,
    .handlers = {node_copy, node_move, node_delete, node_revoke},
    .on_moved = on_node_moved,
    .destructor = node_destructor
};

void *alloc_node(struct heap *heap, size_t slot_bits) {
    // sanity check, limits the number of possible slots in a capability node to ensure (size_t) 1 << slot_bits won't overflow
    if (slot_bits >= sizeof(size_t) * 8) {
        slot_bits = (sizeof(size_t) * 8) - 1;
    }

    size_t total_slots = (size_t) 1 << slot_bits;
    size_t alloc_size = sizeof(struct capability_node) + total_slots * sizeof(struct capability);

    struct capability_node *new = (struct capability_node *) heap_alloc(heap, alloc_size);
    if (new == NULL) {
        return NULL;
    }

    new->slot_bits = slot_bits; // this is guaranteed to fit within a uint8_t due to the above sanity check. if (sizeof(size_t) * 8) - 1 is greater than 256 then you have other problems really
    new->nested_nodes = 1; // to be filled out in populate_capability_slot() if this isn't the kernel root node

    struct capability *slots = &new->capabilities[0];
    for (size_t i = 0; i < total_slots; i ++) {
        (slots ++)->handlers = NULL;
    }

    return new;
}

bool look_up_capability(struct capability *root, size_t address, size_t depth, struct look_up_result *result) {
    if (root->handlers != &node_handlers) {
        return false;
    }

    bool use_first_non_node = false;

    if (depth == SIZE_MAX) {
        use_first_non_node = true;
    } else {
        address &= (((size_t) 1 << depth) - 1); // make sure there aren't any invalid bits outside of the address
    }

    bool should_unlock = heap_lock(root->resource);
    struct capability_node *node = (struct capability_node *) root->resource;
    size_t actual_depth = 0;

    while (1) {
        size_t index_in_node = address & (((size_t) 1 << node->slot_bits) - 1);
        struct capability *slot = &node->capabilities[index_in_node];

        if ((use_first_non_node && slot->handlers != &node_handlers) || depth == node->slot_bits) {
            // finished the search!
            result->slot = slot;
            result->depth = actual_depth + node->slot_bits;
            result->container = node;
            result->should_unlock = should_unlock;
            return true;
        }

        // sanity check. if the depth value doesn't match how many bits are in the node or if the slot doesn't contain a node, give up
        if (depth < node->slot_bits || slot->handlers != &node_handlers) {
            if (should_unlock) {
                heap_unlock(node);
            }
            return false;
        }

        address >>= node->slot_bits;
        depth -= node->slot_bits;
        actual_depth += node->slot_bits;

        bool next_should_unlock = heap_lock(slot->resource);
        struct capability_node *next = (struct capability_node *) slot->resource;

        if (should_unlock) {
            heap_unlock(node);
        }

        should_unlock = next_should_unlock;
        node = next;
    }
}

void unlock_looked_up_capability(struct look_up_result *result) {
    if (result->should_unlock) {
        heap_unlock(result->container);
    }
}

bool look_up_capability_relative(size_t address, size_t depth, struct look_up_result *result) {
    if (scheduler_state.current_thread == NULL) {
        printk("look_up_capability_relative: no current thread!\n");
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

//#ifdef DEBUG
#if 0
static void list_capability_node_slots(struct capability_node *node) {
    size_t last_empty = -1;
    size_t i = 0;
    for (; i < ((size_t) 1 << node->slot_bits); i ++) {
        const struct capability *slot = &node->capabilities[i];

        if (slot->handlers != NULL) {
            if (last_empty != -1) {
                printk("0x%" PRIxPTR " to 0x%" PRIxPTR ": nothing\n", last_empty, i);
                last_empty = -1;
            }

            printk("0x%" PRIxPTR ": %s\n", i, handlers_to_name(slot->handlers));
        } else if (last_empty == -1) {
            last_empty = i;
        }
    }

    if (last_empty != -1) {
        printk("0x%" PRIxPTR " to 0x%" PRIxPTR ": nothing\n", last_empty, i);
    }
}
#endif

size_t populate_capability_slot(struct heap *heap, size_t address, size_t depth, void *resource, struct invocation_handlers *handlers, uint8_t flags) {
    struct look_up_result result;
//#ifdef DEBUG
#if 0
    result.container = NULL;
#endif

    if (!look_up_capability_relative(address, depth, &result)) {
        printk("populate_capability_slot: failed to look up slot at 0x%" PRIxPTR " (%" PRIdPTR " bits)\n", address, depth);

        if ((flags & CAP_FLAG_IS_HEAP_MANAGED) != 0) {
            heap_free(heap, resource);
        }

        return ENOCAPABILITY;
    }

    // make sure destination slot is empty
    if (result.slot->handlers != NULL) {
        printk("populate_capability_slot: slot at 0x%" PRIxPTR " (%" PRIdPTR " bits) isn't empty (contains %s)\n", address, depth, handlers_to_name(result.slot->handlers));

        if ((flags & CAP_FLAG_IS_HEAP_MANAGED) != 0) {
            heap_free(heap, resource);
        }

//#ifdef DEBUG
#if 0
        if (result.container != NULL) {
            list_capability_node_slots(result.container);
        }
#endif

        unlock_looked_up_capability(&result);
        return ECAPEXISTS;
    }

    if (handlers == &node_handlers && result.container != NULL) {
        const struct capability_node *container = (struct capability_node *) result.container;

        // make sure this new node isn't too many layers of nesting deep
        if (container->nested_nodes >= MAX_NESTED_NODES) {
            printk("populate_capability_slot: too many levels of nesting\n");

            if ((flags & CAP_FLAG_IS_HEAP_MANAGED) != 0) {
                heap_free(heap, resource);
            }

            unlock_looked_up_capability(&result);
            return ETOOMUCHNESTING;
        }

        // update the nested nodes count of this new node
        struct capability_node *node = (struct capability_node *) resource;
        node->nested_nodes = container->nested_nodes + 1;
    }

    memset(result.slot, 0, sizeof(struct capability));
    result.slot->handlers = handlers;
    result.slot->resource = resource;
    result.slot->flags = flags | CAP_FLAG_ORIGINAL;
    result.slot->access_rights = UINT8_MAX; // all rights given
    LIST_INIT_NO_CONTAINER(result.slot, resource_list);
    // everything else here assumes NULL is 0

    if (scheduler_state.current_thread != NULL) {
        // get thread id and bucket number from current thread
        result.slot->address.thread_id = scheduler_state.current_thread->thread_id;
        result.slot->address.bucket_number = scheduler_state.current_thread->bucket_number;
    }

    result.slot->address.address = address;
    result.slot->address.depth = result.depth;
    result.slot->heap = heap;

    if ((flags & CAP_FLAG_IS_HEAP_MANAGED) != 0) {
        heap_set_update_capability(resource, &result.slot->address);
        heap_unlock(resource);
    }

    unlock_looked_up_capability(&result);
    return 0;
}

static uint8_t nesting_depth_search(const struct capability *node_capability) {
    const struct capability_node *node = (struct capability_node *) node_capability->resource;
    uint8_t nesting_value = node->nested_nodes;

    const struct capability *capability = &node->capabilities[0];
    for (size_t i = 0; i < ((size_t) 1 << node->slot_bits); i ++, capability ++) {
        if (capability->handlers != &node_handlers) {
            continue;
        }

        uint8_t new_nesting = nesting_depth_search(capability);

        if (new_nesting > nesting_value) {
            nesting_value = new_nesting;
        }
    }

    return nesting_value;
}

uint8_t get_nested_nodes_depth(const struct capability *node_capability) {
    if (node_capability->handlers != &node_handlers) {
        return 0;
    }

    const struct capability_node *node = (struct capability_node *) node_capability->resource;
    return nesting_depth_search(node_capability) - node->nested_nodes;
}

/* ==== untyped memory ==== */

// TODO: wrap these in a mutex

static size_t untyped_lock(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) argument;

    if (!heap_lock(slot->resource)) {
        printk("untyped_lock: attempted to lock a memory region twice!\n");
    }
    return (size_t) slot->resource;
}

static size_t untyped_unlock(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) argument;

    heap_unlock(slot->resource);
    return 0;
}

static size_t untyped_try_lock(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) argument;

    if (heap_lock(slot->resource)) {
        return (size_t) slot->resource;
    } else {
        return (size_t) NULL;
    }
}

static size_t untyped_sizeof(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) argument;

    return heap_sizeof(slot->resource);
}

struct invocation_handlers untyped_handlers = {
    .num_handlers = 4,
    .handlers = {untyped_lock, untyped_unlock, untyped_try_lock, untyped_sizeof},
};

/* ==== address space ==== */

#ifdef DEBUG
const char *type_names[4] = {"untyped", "node", "thread", "endpoint"};
#endif

static size_t address_space_alloc(size_t address, size_t depth, struct capability *slot, size_t argument) {
    const struct alloc_args *args = (struct alloc_args *) argument;

    if (args->type >= 4) {
        printk("address_space_alloc: invalid type %d\n", args->type);
        return EINVAL;
    }

    // make sure there's permission to create this kind of object
    if ((slot->access_rights & (1 << args->type)) == 0) {
        printk(
            "address_space_alloc: capability at 0x%" PRIxPTR " (%" PRIdPTR " bits) doesn't have permission to create type %d (%s)\n",
            address,
            depth,
            args->type,
            type_names[args->type]
        );
        return EPERM;
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

#ifdef DEBUG_CAPABILITIES
    printk(
        "address_space_alloc: allocating resource of type %d (%s) with size %d\n",
        args->type,
        type_names[args->type],
        args->size
    );
#endif

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
        printk("address_space_alloc: heap allocation for type %d (%s) with size %" PRIdPTR " failed\n", args->type, type_names[args->type], args->size);
        return ENOMEM;
    }

#ifdef DEBUG_CAPABILITIES
    printk("address_space_alloc: allocated resource 0x%x with handlers 0x%x\n", resource, handlers);
#endif

    return populate_capability_slot(heap, args->address, args->depth, resource, handlers, CAP_FLAG_IS_HEAP_MANAGED);
}

struct invocation_handlers address_space_handlers = {
    .num_handlers = 1,
    .handlers = {address_space_alloc}
};

/* ==== misc ==== */

void update_capability_resource(const struct absolute_capability_address *address, void *new_resource_address) {
    struct look_up_result result;

    if (address->address == 0 && address->depth == 0) {
        // this capability is the root capability of a thread, look up the thread and use its root capability slot

        struct thread_capability *thread;
        result.should_unlock = look_up_thread_by_id(address->thread_id, address->bucket_number, &thread);
        result.container = (void *) thread;

        if (thread == NULL) {
            printk("update_capability_resource: couldn't find thread 0x%x in bucket %d\n", address->thread_id, address->bucket_number);
            return;
        }

        result.slot = &thread->root_capability;
    } else if (!look_up_capability_absolute(address, &result)) {
        printk("update_capability_resource: couldn't locate capability at 0x%x:0x%" PRIxPTR " (%" PRIdPTR " bits)\n", address->thread_id, address->address, address->depth);
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

size_t invoke_capability(size_t address, size_t depth, size_t handler_number, size_t argument) {
    struct look_up_result result;

    if (!look_up_capability_relative(address, depth, &result)) {
        printk("invoke_capability: couldn't locate capability at 0x%" PRIxPTR " (%" PRIdPTR " bits) for invocation\n", address, depth);

        // TODO: rethink how error values are returned here in order to prevent issues with untyped_lock invocations not returning NULL
        return ENOCAPABILITY;
    }

    struct invocation_handlers *handlers = result.slot->handlers;

    // TODO: add guard value to invocation numbers so you can't accidentally run a different invocation than was intended
    if (handlers == NULL || handler_number >= handlers->num_handlers) {
        printk("invoke_capability: invocation %" PRIdPTR " on capability 0x%" PRIxPTR " (%" PRIdPTR " bits, type %s) is invalid\n", handler_number, address, depth, handlers_to_name(handlers));
        unlock_looked_up_capability(&result);

        // TODO: see above
        return ECAPINVAL;
    }

#ifdef DEBUG_CAPABILITIES
    printk("invoke_capability: invoking %" PRIdPTR " on 0x%" PRIxPTR " (%" PRIdPTR " bits) with argument 0x%" PRIxPTR "\n", handler_number, address, depth, argument);
#endif

    size_t return_value = handlers->handlers[handler_number](address, depth, result.slot, argument);
    unlock_looked_up_capability(&result);

#ifdef DEBUG_CAPABILITIES
    printk("invoke_capability: invocation returned 0x%" PRIxPTR "\n", return_value);
#endif

    return return_value;
}
