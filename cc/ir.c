#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
        case TOP_BASIC:
            switch (type->type.basic.storage_class) {
                case C_EXTERN:
                    printf("extern ");
                    break;
                case C_STATIC:
                    printf("static ");
                    break;
            }
            if (type->type.basic.const_qualified)
                printf("const ");
            if (type->type.basic.volatile_qualified)
                printf("volatile ");
            switch (type->type.basic.sign_specifier) {
                case S_SIGNED:
                    printf("signed ");
                    break;
                case S_UNSIGNED:
                    printf("unsigned ");
                    break;
            }
            switch (type->type.basic.type_specifier) {
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
                        type->type.basic.type_specifier == TY_STRUCT ?
                        "struct " : "union "
                    );
                    if (type->type.basic.has_fields) {
                        struct struct_union_field *current =
                            type->type.basic.name_field_ptr;

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
                        printf("%s ", type->type.basic.name_field_ptr);
                    break;
                case TY_ENUM:
                    break;
            }
            if (name == NULL)
                printf("(unnamed)\n");
            else
                printf("`%s'\n", name);
            break;
        case TOP_ARRAY:
            printf("array[%d] -> ", type->type.derived.type.array.length);
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
        case TOP_POINTER:
            if (type->type.derived.type.pointer.const_qualified)
                printf("const ");
            if (type->type.derived.type.pointer.volatile_qualified)
                printf("volatile ");
            printf("pointer -> ");
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
        case TOP_FUNCTION:
            printf("function (\n");
            for (f = type->type.derived.type.function; f != NULL; f = f->next)
                print_type_internal(f->type, f->name, indent + 4, 1);
            printf(") -> ");
            print_type_internal(type->type.derived.derivation, name, indent, 0);
            break;
    }
}

void print_type(struct type *type, const char *name) {
    print_type_internal(type, name, 0, 0);
}

void free_type(struct type *type) {
    struct type *next = type;

    while (next != NULL) {
        if (type->top == TOP_BASIC)
            next = NULL;
        else
            next = type->type.derived.derivation;

        if (type->references <= 1) {
            if (type->top == TOP_BASIC) {
                /* free struct/union field names/types */
                if (type->type.basic.name_field_ptr != NULL)
                    if (type->type.basic.has_fields) {
                        struct struct_union_field *current =
                            type->type.basic.name_field_ptr,
                            *next;

                        while (current != NULL) {
                            next = current->next;

                            free_type(current->type);

                            if (current->name != NULL)
                                free((void *) current->name);

                            free(current);

                            current = next;
                        }
                    } else
                        free(type->type.basic.name_field_ptr);
            } else if (type->top == TOP_FUNCTION) {
                /* free function argument names/types */
                struct function_type *current = type->type.derived.type.function,
                    *next;

                while (current != NULL) {
                    next = current->next;

                    free_type(current->type);

                    if (current->name != NULL)
                        free((void *) current->name);

                    free(current);

                    current = next;
                }
            }

            free(type);

            type = next;
        } else {
            type->references --;
            break;
        }
    }
}
