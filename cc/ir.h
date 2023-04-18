#ifndef IR_H
#define IR_H

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

struct standard_type {
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
    struct type *derivation;
    struct function_type *next;
};

enum top_type {
    TOP_NORMAL = 0,
    TOP_ARRAY = 1,
    TOP_POINTER = 2,
    TOP_FUNCTION = 3,
};

struct type {
    enum top_type top;
    union {
        struct standard_type standard;
        struct {
            struct type *derivation;
            union {
                struct array_type array;
                struct pointer_type pointer;
                struct function_type function;
            } type;
        } derived;
    } type;
    unsigned int size;
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

#endif
