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
            for (i = 0; i < indent; i ++)
                printf(" ");
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
                struct function_type *current =
                    type->type.derived.type.function,
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

static void label_node(struct node *node) {
    printf("n%x [label=\"", node);
    switch (node->kind) {
        case N_LITERAL:
            printf("%d", node->data.literal);
            break;
        case N_CALL:
            printf("call %s", node->data.tag);
            break;
        case N_MUL:
            printf("*");
            break;
        case N_DIV:
            printf("/");
            break;
        case N_MOD:
            printf("%%");
            break;
        case N_ADD:
            printf("+");
            break;
        case N_SUB:
            printf("-");
            break;
        case N_SHIFT_LEFT:
            printf("<<");
            break;
        case N_SHIFT_RIGHT:
            printf(">>");
            break;
        case N_LESS:
            printf("<");
            break;
        case N_GREATER:
            printf(">");
            break;
        case N_LESS_EQ:
            printf("<=");
            break;
        case N_GREATER_EQ:
            printf(">=");
            break;
        case N_EQUALS:
            printf("==");
            break;
        case N_NOT_EQ:
            printf("!=");
            break;
        case N_BITWISE_AND:
            printf("&");
            break;
        case N_BITWISE_XOR:
            printf("^");
            break;
        case N_BITWISE_OR:
            printf("|");
            break;
        case N_AND:
            printf("&&");
            break;
        case N_OR:
            printf("||");
            break;
        case N_LVALUE:
            printf("lvalue");
            break;
        default:
            printf("(unknown)");
            break;
    }
    printf("\"]");

    if (node->has_side_effects)
        printf(" [shape=box]");

    printf("; ");
}

static void debug_graph_recurse(struct node *node) {
    switch (node->kind) {
        case N_MUL:
        case N_DIV:
        case N_MOD:
        case N_ADD:
        case N_SUB:
        case N_SHIFT_LEFT:
        case N_SHIFT_RIGHT:
        case N_LESS:
        case N_GREATER:
        case N_LESS_EQ:
        case N_GREATER_EQ:
        case N_EQUALS:
        case N_NOT_EQ:
        case N_BITWISE_AND:
        case N_BITWISE_XOR:
        case N_BITWISE_OR:
        case N_AND:
        case N_OR:
            if (!node->deps.op.left->visited) {
                node->deps.op.left->visited = 1;
                label_node(node->deps.op.left);
                debug_graph_recurse(node->deps.op.left);
            }
            if (!node->deps.op.right->visited) {
                node->deps.op.right->visited = 1;
                label_node(node->deps.op.right);
                debug_graph_recurse(node->deps.op.right);
            }
            printf("n%x -> n%x [label=right]; ", node, node->deps.op.right);
            printf("n%x -> n%x [label=left]; ", node, node->deps.op.left);
            break;
        case N_LVALUE:
            label_node(node->deps.single);
            printf("n%x -> n%x; ", node, node->deps.single);
            debug_graph_recurse(node->deps.single);
            break;
    }

    if (node->prev != NULL) {
        if (!node->prev->visited) {
            node->prev->visited = 1;
            label_node(node->prev);
            debug_graph_recurse(node->prev);
        }
        printf("n%x -> n%x [style=dotted]; ", node, node->prev);
    }
}

void debug_graph(struct node *node) {
    printf("digraph { ");
    if (node != NULL) {
        label_node(node);
        debug_graph_recurse(node);
    }
    printf("}\n");
}
