#pragma once

/* ==== linked lists with a container completely separate from the list elements ==== */

#define LIST_LINK(type) \
struct { \
    type *prev; \
    type *next; \
}

// TODO: make this accept the name of the struct instead of just any type name
#define LIST_CONTAINER(type) \
struct { \
    type *start; \
    type *end; \
}

#define LIST_INIT(container) \
do { \
    (container).start = NULL; \
    (container).end = NULL; \
} while (0)

/// appends an item to the end of a linked list with a separate container
#define LIST_APPEND(container, field, item) \
if ((container).end == NULL) { \
    (container).start = (item); \
    (container).end = (item); \
    (item)->field.prev = NULL; \
    (item)->field.next = NULL; \
} else { \
    (container).end->field.next = (item); \
    (item)->field.prev = (container).end; \
    (item)->field.next = NULL; \
    (container).end = (item); \
}

/// updates the address of an item in a linked list with a separate container
#define LIST_UPDATE_ADDRESS(container, field, item) \
do { \
    if ((item)->field.next == NULL) { \
        (container).end = (item); \
    } else { \
        (item)->field.next->field.prev = (item); \
    } \
    if ((item)->field.prev == NULL) { \
        (container).start = (item); \
    } else { \
        (item)->field.prev->field.next = (item); \
    } \
} while(0)

/// pops the first item in a linked list with a separate container
#define LIST_POP_FROM_START(container, field, variable) \
if ((container).start != NULL) { \
    (variable) = (container).start; \
    (container).start = (variable)->field.next; \
    (variable)->field.prev = NULL; \
    (variable)->field.next = NULL; \
    if ((container).start == NULL) { \
        (container).end = NULL; \
    } \
}

/// checks whether a linked list with a separate container can be popped from
#define LIST_CAN_POP(container) ((container).start != NULL)

/// removes an item from a linked list with a separate container
#define LIST_REMOVE(container, field, item) \
do { \
    if ((item)->field.prev == NULL) { \
        (container).start = (item)->field.next; \
    } else { \
        (item)->field.prev->field.next = (item)->field.next; \
    } \
    if ((item)->field.next == NULL) { \
        (container).end = (item)->field.prev; \
    } else { \
        (item)->field.next->field.prev = (item)->field.prev; \
    } \
} while (0)

/// iterates over a linked list with a separate container
#define LIST_ITER(type, container, field, variable) \
for (type *variable = (container).start; variable != NULL; variable = variable->field.next)

/* ==== linked lists with the container in all the elements ==== */

#define LIST_NO_CONTAINER(type) \
struct { \
    type *start; \
    type *end; \
    type *prev; \
    type *next; \
}

#define LIST_INIT_NO_CONTAINER(container, field) \
do { \
    (container)->field.start = (container); \
    (container)->field.end = (container); \
    (container)->field.prev = NULL; \
    (container)->field.next = NULL; \
} while (0)

/// \brief appends an item to the end of a linked list with no container
///
/// this operation is O(n) because it has to update every link in the list, and should therefore be avoided if possible
#define LIST_APPEND_NO_CONTAINER(type, container, field, item) \
do { \
    (container)->field.end->field.next = (item); \
    (item)->field.prev = (container)->field.end; \
    (item)->field.next = NULL; \
    (item)->field.start = (container)->field.start; \
    LIST_ITER_NO_CONTAINER(type, field, container, link) { \
        link->field.end = (item); \
    } \
} while (0)

/// \brief inserts an item to an arbitrary position in a linked list with no container
///
/// if the list contains a single item, the item will be inserted at the end of the list.
/// if the list contains more than one item, the item will be inserted between the second-to-last and last items in the list
#define LIST_INSERT_NO_CONTAINER(type, container, field, item) \
if ((container)->field.start == (container)->field.end) { \
    /* there's only one item in the list, add to the end */ \
    (container)->field.end = (item); \
    (item)->field.start = (container); \
    (item)->field.end = (item); \
    (container)->field.next = (item); \
    (item)->field.prev = (container); \
    (item)->field.next = NULL; \
} else { \
    /* there's more than one item in the list, insert between the end link and the link before that one */ \
    (item)->field.start = (container)->field.start; \
    (item)->field.end = (container)->field.end; \
    type *prev = (container)->field.end->field.prev; \
    prev->field.next = (item); \
    (item)->field.prev = (prev); \
    (item)->field.next = (container)->field.end; \
}

/// updates the address of an item in a linked list with no container
#define LIST_UPDATE_ADDRESS_NO_CONTAINER(type, field, item) \
do { \
    if ((item)->field.prev != NULL) { \
        (item)->field.prev->field.next = (item); \
    } \
    if ((item)->field.next != NULL) { \
        (item)->field.next->field.prev = (item); \
    } \
    /* these are afterwards so that any references to this link in the list are valid */ \
    if ((item)->field.prev == NULL) { \
        for (type *link = (item); link != NULL; link = link->field.next) { \
            link->field.start = (item); \
        } \
    } \
    if ((item)->field.next == NULL) { \
        for (type *link = (item); link != NULL; link = link->field.prev) { \
            link->field.end = (item); \
        } \
    } \
} while (0)

/// iterates over a linked list with no container
#define LIST_ITER_NO_CONTAINER(type, field, item, variable) \
for (type *variable = (item)->field.start; variable != NULL; variable = variable->field.next)

/// deletes an item from a linked list with no container
#define LIST_DELETE_NO_CONTAINER(type, field, item) \
if ((item)->field.prev != NULL && (item)->field.next != NULL) { \
    /* this link is in the middle of its list, so nothing special needs to happen, just connect the previous and next links */ \
    (item)->field.prev->field.next = (item)->field.next; \
    (item)->field.next->field.prev = (item)->field.prev; \
} else { \
    /* this is either the start or end of the list, so all the links in the list need to be updated */ \
    if ((item)->field.prev == NULL && (item)->field.next != NULL) { \
        type *start = (item)->field.next; \
        start->field.prev = NULL; \
        for (type *link = start; link != NULL; link = link->field.next) { \
            link->field.start = start; \
        } \
    } \
    if ((item)->field.next == NULL && (item)->field.prev != NULL) { \
        type *end = (item)->field.prev; \
        end->field.next = NULL; \
        for (type *link = end; link != NULL; link = link->field.prev) { \
            link->field.end = end; \
        } \
    } \
}
