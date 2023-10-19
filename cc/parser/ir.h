#ifndef IR_H
#define IR_H

#include <stdint.h>
#include <sys/types.h>

/* IR graph representation */

enum node_kind {
    /* no dependencies */
    N_LITERAL = 0,
    N_CALL,
    /* 1 dependency */
    N_RETURN,
    N_REFERENCE,
    N_DEREF,
    N_NEGATE,
    N_BITWISE_NOT,
    N_NOT,
    /* 2 dependencies */
    N_MUL,
    N_DIV,
    N_MOD,
    N_ADD,
    N_SUB,
    N_SHIFT_LEFT,
    N_SHIFT_RIGHT,
    N_LESS,
    N_GREATER,
    N_LESS_EQ,
    N_GREATER_EQ,
    N_EQUALS,
    N_NOT_EQ,
    N_BITWISE_AND,
    N_BITWISE_XOR,
    N_BITWISE_OR,
    N_AND,
    N_OR,
    N_DEREF_ASSIGN,
};

#define NODE_VISITED 1
#define NODE_IS_ARITH_TYPE 2
#define NODE_IS_LVALUE 4
#define NODE_LEFT_VISITED 8
#define NODE_RIGHT_VISITED 16
#define NODE_PREV_VISITED 32

struct node {
    /* what kind of node this is */
    enum node_kind kind;
    /* the file offset of the left-hand side or single argument node */
    off_t left;
    /* the file offset of the right-hand side argument node (if applicable) */
    off_t right;
    /* the file offset of the previous node with side effects */
    off_t prev;
    /* the file offset of the next node in the topological sort */
    off_t sorted_next;
    /* the file offset of the type of this node */
    off_t type;
    /* how many other nodes reference this node */
    int16_t references;
    /* how many times this node has been visited */
    uint16_t visits;
    /* the register number that has been allocated for this node */
    uint8_t reg_num;
    /* flags lmao */
    uint8_t flags;
};

/* information about a node that was written to disk */
struct written_node {
    /* the offset in the file at which the node was written */
    off_t offset;
    /* the kind of node that was written */
    enum node_kind kind;
    /* the contents of the flags field from the node */
    uint8_t flags;
    /* the offset in the file of the node's type, if applicable */
    off_t type;
    /* if this written node is an lvalue pointing to the value of a variable,
     * this is a pointer to the variable whose value was referenced */
    struct variable *variable;
};

void debug_graph(struct node *node);
void debug_sorted(struct node *node);

/* frees memory associated with a node with proper reference counting */
void free_node(struct node *node);

/* an entry in a linked list of scopes. if this scope has no variables declared,
 * `variables` will be set to NULL */
struct scope {
    struct hashtable *variables;
    off_t last_side_effect;
    struct scope *next;
};

#define VAR_IS_ARITH_TYPE 1

struct variable {
    /* what type this variable is */
    struct type *type;
    /* if the type has been written to disk, this is its offset */
    off_t written_type;
    /* the node containing the value last assigned to the variable */
    off_t last_assignment;
    /* stores the last assignments made to this variable in scopes other than
     * the one it was created in */
    struct phi_list *phi_list;
    /* if this variable didn't originate in the current scope, this points to
     * the instance of the variable in the scope it originated from */
    struct variable *parent;
    uint16_t references;
    uint8_t flags;
};

struct phi_list {
    struct node *last_assignment;
    struct phi_list *next;
};

/* frees memory associated with a variable with proper reference counting */
void free_variable(struct variable *variable);

struct node *topological_sort(struct node *node);

#endif
