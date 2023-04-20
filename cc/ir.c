#include <stddef.h>
#include <stdio.h>
#include "ir.h"

static void print_type_internal(
    struct type *type,
    const char *name,
    unsigned char indent,
    char do_indent
) {
    unsigned char i;
    struct function_type *f;
    if (do_indent)
        for (i = 0; i < indent; i ++)
            printf(" ");

    if (type == NULL) {
        printf("(null pointer)\n");
        return;
    }

    switch (type->top) {
        case TOP_NORMAL:
            switch (type->type.standard.storage_class) {
                case C_EXTERN:
                    printf("extern ");
                    break;
                case C_STATIC:
                    printf("static ");
                    break;
            }
            if (type->type.standard.const_qualified)
                printf("const ");
            if (type->type.standard.volatile_qualified)
                printf("volatile ");
            switch (type->type.standard.sign_specifier) {
                case S_SIGNED:
                    printf("signed ");
                    break;
                case S_UNSIGNED:
                    printf("unsigned ");
                    break;
            }
            switch (type->type.standard.type_specifier) {
                case TY_VOID:
                    printf("void ");
                    break;
                case TY_CHAR:
                    printf("char ");
                    break;
                case TY_SHORT:
                    printf("short ");
                    break;
                case TY_INT:
                    printf("int ");
                    break;
                case TY_LONG:
                    printf("long ");
                    break;
                case TY_LONG_LONG:
                    printf("long long ");
                    break;
                case TY_STRUCT:
                case TY_UNION:
                    printf(
                        type->type.standard.type_specifier == TY_STRUCT ?
                        "struct " : "union "
                    );
                    if (type->type.standard.has_fields) {
                        struct struct_union_field *current =
                            type->type.standard.name_field_ptr;

                        printf("{\n");
                        for (; current != NULL; current = current->next) {
                            print_type_internal(
                                current->type,
                                current->name,
                                indent + 4,
                                1
                            );
                        }
                        for (i = 0; i < indent; i ++)
                            printf(" ");
                        printf("} ");
                    } else
                        printf("%s ", type->type.standard.name_field_ptr);
                    break;
                case TY_ENUM:
                    break;
            }
            if (name == NULL)
                printf("\n");
            else
                printf("`%s'\n", name);
            break;
        case TOP_ARRAY:
            printf("array[%d] -> ", type->type.derived.type.array.length);
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
        case TOP_POINTER:
            printf("pointer -> ");
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
        case TOP_FUNCTION:
            printf("function (");
            for (
                f = &type->type.derived.type.function;
                f != NULL;
                f = f->next
            )
                print_type_internal(f->type, f->name, indent, 0);
            printf(") -> ");
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
    }
}

void print_type(struct type *type, const char *name) {
    print_type_internal(type, name, 0, 0);
}

#if 0
unsigned int get_type_size(struct type *type) {
    struct struct_union_field *field;

    switch (type->top_type) {
        case TOP_NORMAL:
            switch (type->type.standard_type.type_specifier) {
                case TY_VOID:
                    return 0;
                case TY_CHAR:
                    return 1;
                case TY_SHORT:
                case TY_INT:
                    return 2;
                case TY_LONG:
                    return 4;
                case TY_LONG_LONG:
                    return 8;
                case TY_STRUCT:
                    if (type->type.standard_type.has_fields) {
                        field = type->type.standard_type.name_field_ptr.field;

                        if (field == NULL)
                            return 0;

                        while (1)
                            if (field->next == NULL)
                                return field->offset +
                                    get_type_size(&field->type);
                            else
                                field = field->next;
                    } else
                        return 0;
                case TY_UNION:
                    if (type->type.standard_type.has_fields) {
                        unsigned int largest = 0;

                        field = type->type.standard_type.name_field_ptr.field;
                        while (field != NULL) {
                            unsigned int size = get_type_size(&field->type);
                            if (size > largest)
                                largest = size;

                            field = field->next;
                        }

                        return largest;
                    } else
                        return 0;
                case TY_ENUM:
                    return 0;
            }
        case TOP_ARRAY:
            return type->type.array_type.length *
                get_type_size(type->type.array_type.derivation);
        case TOP_POINTER:
            return 2;
        case TOP_FUNCTION:
            return 0; /* size of function is unknown */
    }
}
#endif
