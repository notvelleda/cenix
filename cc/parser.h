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

enum type_qualifier {
    Q_NONE = 0,
    Q_CONST = 1,
    Q_VOLATILE = 2,
};

struct standard_type {
    enum storage_class storage_class: 2;
    enum sign_specifier sign_specifier: 2;
    enum type_specifier type_specifier: 4;
    enum type_qualifier type_qualifier: 2;
    /* optional, only used for struct, union, enum. set to NULL otherwise */
    const char *name;
};

struct array_type {
    struct type *derivation;
    unsigned int length;
};

struct pointer_type {
    struct type *derivation;
    enum type_qualifier qualifier;
};

/* first link in the list is the return type, all subsequent links are arguments
 */
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
    enum top_type top_type;
    union {
        struct standard_type standard_type;
        struct array_type array_type;
        struct pointer_type pointer_type;
        struct function_type function_type;
    } type;
};
