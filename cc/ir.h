#ifndef IR_H
#define IR_H

/* IR types */

enum storage_class {
    C_AUTO = 0, /* keyword for this isn't supported since it's the default */
    C_EXTERN = 1,
    C_STATIC = 2,
    /* register is unsupported */
};

enum sign_specifier {
    S_UNKNOWN = 0,
    S_UNSIGNED = 1,
    S_SIGNED = 2,
};

enum type_specifier {
    TY_VOID = 0,
    TY_CHAR = 1,
    TY_SHORT = 2,
    TY_INT = 3,
    TY_LONG = 4,
    TY_LONG_LONG = 5,
    TY_STRUCT = 6,
    TY_UNION = 7,
    TY_ENUM = 8,
};

struct basic_type {
    enum storage_class storage_class: 2;
    enum sign_specifier sign_specifier: 2;
    enum type_specifier type_specifier: 4;
    unsigned char const_qualified: 1;
    unsigned char volatile_qualified: 1;
    /* whether name_field_ptr points to a linked list of struct_union_fields or
     * just a string */
    unsigned char has_fields: 1;
    void *name_field_ptr;
};

struct array_type {
    unsigned int length;
};

struct pointer_type {
    unsigned char const_qualified: 1;
    unsigned char volatile_qualified: 1;
};

/* each link in the list is an argument, if both fields are NULL there are
 * no arguments */
struct function_type {
    struct type *type;
    const char *name;
    struct function_type *next;
};

enum top_type {
    TOP_BASIC = 0,
    TOP_ARRAY = 1,
    TOP_POINTER = 2,
    TOP_FUNCTION = 3,
};

struct type {
    enum top_type top;
    union {
        struct basic_type basic;
        struct {
            struct type *derivation;
            union {
                struct array_type array;
                struct pointer_type pointer;
                struct function_type *function;
            } type;
        } derived;
    } type;
    unsigned int size;
    unsigned int references;
};

struct struct_union_field {
    /* the type of this field */
    struct type *type;
    /* the name of this field */
    const char *name;
    /* the hashed name of this field to speed up comparisons */
    unsigned long name_hash;
    /* the offset of this field in a struct */
    unsigned int offset;
    /* the next field in the list, or NULL if it's the end */
    struct struct_union_field *next;
};

/* prints a type and its derivations, useful for debugging */
void print_type(struct type *type, const char *name);

/* frees memory allocated for a type with proper reference counting. make sure
 * to decrement the references field before calling if a reference is removed */
void free_type(struct type *type);

/* IR graph representation */

enum node_kind {
    /* no dependencies */
    N_LITERAL = 0,
    N_CALL,
    /* 1 dependency */
    N_LVALUE,
    N_RETURN,
    N_PRE_INC,
    N_PRE_DEC,
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

struct op_edges {
    struct node *left;
    struct node *right;
};

#define NODE_VISITED 1
#define NODE_IS_ARITH_TYPE 2
#define NODE_HAS_SIDE_EFFECTS 4
#define NODE_IS_LVALUE 8
#define NODE_LEFT_VISITED 16
#define NODE_RIGHT_VISITED 32
#define NODE_PREV_VISITED 64

struct node {
    enum node_kind kind;
    union {
        struct node *single;
        struct op_edges op;
    } deps;
    struct node *prev;
    struct node *sorted_next;
    union {
        const char *tag;
        unsigned long long literal;
        struct variable *lvalue;
    } data;
    struct type *type;
    unsigned int references;
    unsigned int visits;
    unsigned char reg_num;
    unsigned char flags;
};

void debug_graph(struct node *node);
void debug_sorted(struct node *node);

/* frees memory associated with a node with proper reference counting */
void free_node(struct node *node);

/* an entry in a linked list of scopes. if this scope has no variables declared,
 * `variables` will be set to NULL */
struct scope {
    struct hashtable *variables;
    struct node *last_side_effect;
    struct scope *next;
};

#define VAR_IS_ARITH_TYPE 1

struct variable {
    /* what type this variable is */
    struct type *type;
    /* the first place a value is read from this variable */
    struct node *first_load;
    /* the node containing the value last assigned to the variable */
    struct node *last_assignment;
    /* stores the last assignments made to this variable in scopes other than
     * the one it was created in */
    struct phi_list *phi_list;
    /* if this variable didn't originate in the current scope, this points to
     * the instance of the variable in the scope it originated from */
    struct variable *parent;
    unsigned int references;
    unsigned char flags;
};

struct phi_list {
    struct node *last_assignment;
    struct phi_list *next;
};

/* frees memory associate with a variable with proper reference counting */
void free_variable(struct variable *variable);

struct node *topological_sort(struct node *node);

#endif
