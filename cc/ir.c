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
        if (type->references != 0)
            break;

        if (type->top == TOP_BASIC)
            next = NULL;
        else
            next = type->type.derived.derivation;

        if (type->top == TOP_BASIC) {
            /* free struct/union field names/types */
            if (type->type.basic.name_field_ptr != NULL)
                if (type->type.basic.has_fields) {
                    struct struct_union_field *current =
                        type->type.basic.name_field_ptr,
                        *next;

                    while (current != NULL) {
                        next = current->next;

                        current->type->references --;
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

                current->type->references --;
                free_type(current->type);

                if (current->name != NULL)
                    free((void *) current->name);

                free(current);

                current = next;
            }
        }

        free(type);

        type = next;
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
        case N_LVALUE:
            printf("lvalue");
            break;
        case N_RETURN:
            printf("return");
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
        default:
            printf("(unknown)");
            break;
    }
    printf(" (%d references)", node->references);
    printf("\"]");

    if (node->has_side_effects)
        printf(" [shape=box]");

    printf("; ");
}

static void debug_graph_recurse(struct node *node) {
    if (node->kind >= N_MUL && node->kind <= N_OR) {
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
    } else if (node->kind >= N_LVALUE && node->kind <= N_RETURN) {
        if (!node->deps.single->visited) {
            node->deps.single->visited = 1;
            label_node(node->deps.single);
            debug_graph_recurse(node->deps.single);
        }
        printf("n%x -> n%x; ", node, node->deps.single);
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

void free_node(struct node *node) {
    if (node == NULL)
        return;

    while (1) {
        struct node *next = NULL;

        if (node->references != 0)
            break;

        if (node->kind >= N_MUL && node->kind <= N_OR) {
            next = node->deps.op.left;
            node->deps.op.right->references --;
            free_node(node->deps.op.right);
        } else if (node->kind >= N_LVALUE && node->kind <= N_RETURN)
            next = node->deps.single;

        if (node->prev != NULL)
            if (next == NULL)
                next = node->prev;
            else {
                node->prev->references --;
                free_node(node->prev);
            }

        if (node->type != NULL) {
            node->type->references --;
            free_type(node->type);
        }

        if (node->kind == N_CALL)
            free((void *) node->data.tag);
        else if (node->kind == N_LVALUE) {
            node->data.lvalue->references --;
            free_variable(node->data.lvalue);
        }

        free(node);

        if (next == NULL)
            break;

        node = next;
        node->references --;
    }
}

void free_variable(struct variable *variable) {
    struct phi_list *current;

    if (variable == NULL)
        return;

    if (variable->references == 1 && variable->last_assignment != NULL) {
        variable->last_assignment->references --;
        free_node(variable->last_assignment);
        return;
    }

    if (variable->references != 0)
        return;

    /*fprintf(stderr, "freeing variable %x\n", variable);*/

    if (variable->type != NULL) {
        variable->type->references --;
        free_type(variable->type);
    }
    if (variable->first_load != NULL) {
        variable->first_load->references --;
        free_node(variable->first_load);
    }
    if (variable->last_assignment != NULL) {
        variable->last_assignment->references --;
        free_node(variable->last_assignment);
    }
    for (
        current = variable->phi_list;
        current != NULL;
        current = current->next
    ) {
        current->last_assignment->references --;
        free_node(current->last_assignment);
    }
    if (variable->parent != NULL) {
        variable->parent->references --;
        fprintf(stderr, "%d parent references\n", variable->parent->references);
        /* this call is probably useless */
        free_variable(variable->parent);
    }
    free(variable);
}
