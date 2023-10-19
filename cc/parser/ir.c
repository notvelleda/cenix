#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "ir.h"

#if 0
static void label_node(struct node *node) {
    printf("n%x [label=\"", node);
    switch (node->kind) {
        case N_LITERAL:
            printf("%d", node->data.literal);
            break;
        case N_CALL:
            printf("call %s", node->data.tag);
            break;
        case N_RETURN:
            printf("return");
            break;
        case N_REFERENCE:
            printf("reference");
            break;
        case N_DEREF:
            printf("deref");
            break;
        case N_NEGATE:
            printf("-");
            break;
        case N_BITWISE_NOT:
            printf("~");
            break;
        case N_NOT:
            printf("!");
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
        case N_DEREF_ASSIGN:
            printf("deref assign");
            break;
        default:
            printf("(unknown)");
            break;
    }
    printf(" (r %d, v %d)", node->references, node->visits);
    printf("\"]");

    if (node->flags & NODE_HAS_SIDE_EFFECTS)
        printf(" [shape=box]");

    printf("; ");
}

static void debug_graph_recurse(struct node *node) {
    if (node->kind >= N_MUL) {
        /* handle 2 dependency nodes */
        if (node->deps.op.left != NULL) {
            if (!(node->deps.op.left->flags & NODE_VISITED)) {
                node->deps.op.left->flags |= NODE_VISITED;
                label_node(node->deps.op.left);
                debug_graph_recurse(node->deps.op.left);
            }
            printf("n%x -> n%x [label=left]; ", node, node->deps.op.left);
        }
        if (node->deps.op.right != NULL) {
            if (!(node->deps.op.right->flags & NODE_VISITED)) {
                node->deps.op.right->flags |= NODE_VISITED;
                label_node(node->deps.op.right);
                debug_graph_recurse(node->deps.op.right);
            }
            printf("n%x -> n%x [label=right]; ", node, node->deps.op.right);
        }
    } else if (node->kind >= N_RETURN) {
        /* handle 1 dependency nodes */
        if (node->deps.single != NULL) {
            if (!(node->deps.single->flags & NODE_VISITED)) {
                node->deps.single->flags |= NODE_VISITED;
                label_node(node->deps.single);
                debug_graph_recurse(node->deps.single);
            }
            printf("n%x -> n%x; ", node, node->deps.single);
        }
    }

    if (node->prev != NULL) {
        if (!(node->prev->flags & NODE_VISITED)) {
            node->prev->flags |= NODE_VISITED;
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

void debug_sorted(struct node *node) {
    struct node *cur;

    printf(
        "digraph { "
        "rankdir=LR; graph [ordering=out]; "
        "subgraph { "
        "rank=same; "
    );

    for (cur = node; cur != NULL; cur = cur->sorted_next) {
        label_node(cur);

        if (cur->kind >= N_MUL) {
            /* handle 2 dependency nodes */
            printf(
                "n%x -> n%x [label=right] [constraint=false]; ",
                cur,
                cur->deps.op.right
            );
            printf(
                "n%x -> n%x [label=left] [constraint=false]; ",
                cur,
                cur->deps.op.left
            );
        } else if (cur->kind >= N_RETURN)
            /* handle 1 dependency nodes */
            printf("n%x -> n%x [constraint=false]; ", cur, cur->deps.single);

        if (cur->prev != NULL)
            printf(
                "n%x -> n%x [constraint=false] [style=dotted]; ",
                cur,
                cur->prev
            );
    }

    /* add additional ordering edges because dot is wacky */
    if (node != NULL) {
        printf("n%x", node);
        for (cur = node->sorted_next; cur != NULL; cur = cur->sorted_next)
            printf(" -> n%x", cur);
        printf(" [style=invis]; ");
    }

    printf("}}\n");
}

void free_node(struct node *node) {
    if (node == NULL)
        return;

    while (1) {
        struct node *next = NULL;

        if (node->references != 0)
            break;

        if (node->kind >= N_MUL) {
            /* handle 2 dependency nodes */
            next = node->deps.op.left;
            node->deps.op.right->references --;
            free_node(node->deps.op.right);
        } else if (node->kind >= N_RETURN)
            /* handle 1 dependency nodes */
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

struct node_list {
    struct node *head;
    struct node *tail;
    unsigned int longest_path;
};

static void join_lists(struct node_list *a, struct node_list *b) {
    if (a->tail == NULL) {
        if (b->head != NULL) {
            a->head = b->head;
            a->tail = b->tail;
        }
        return;
    } else if (b->head == NULL)
        return;

    (a->tail->sorted_next = b->head)->references ++;
    a->tail = b->tail;
}

static void sort_2_lists(
    struct node_list *parent,
    struct node_list *a,
    struct node_list *b
) {
    if (a->tail == NULL)
        join_lists(parent, b);
    else if (b->tail == NULL)
        join_lists(parent, a);
    else if (a->longest_path >= b->longest_path) {
        join_lists(a, b);
        join_lists(parent, a);
    } else {
        join_lists(b, a);
        join_lists(parent, b);
    }
}

/* modified version of https://stackoverflow.com/a/31939937 */
static unsigned int sort_3_lists(
    struct node_list *parent,
    struct node_list *a,
    struct node_list *b,
    struct node_list *c
) {
    if (b->longest_path < c->longest_path) {
        unsigned int longest = c->longest_path;

        if (a->longest_path < c->longest_path) {
            if (a->longest_path < b->longest_path) {
                /* ordering is c, b, a */
                join_lists(parent, c);
                join_lists(parent, b);
                join_lists(parent, a);
            } else {
                /* ordering is c, a, b */
                join_lists(parent, c);
                join_lists(parent, a);
                join_lists(parent, b);
            }
        } else {
            /* ordering is a, c, b */
            join_lists(parent, a);
            join_lists(parent, c);
            join_lists(parent, b);
        }

        return longest;
    } else {
        unsigned int longest = b->longest_path;

        if (a->longest_path < b->longest_path) {
            if (a->longest_path < c->longest_path) {
                /* ordering is b, c, a */
                join_lists(parent, b);
                join_lists(parent, c);
                join_lists(parent, a);
            } else {
                /* ordering is b, a, c */
                join_lists(parent, b);
                join_lists(parent, a);
                join_lists(parent, c);
            }
        } else {
            /* ordering is a, b, c */
            join_lists(parent, a);
            join_lists(parent, b);
            join_lists(parent, c);
        }

        return longest;
    }
}

static void sort_visit(
    struct node *node,
    struct node_list *list,
    unsigned char ignore_visit
) {
    struct node_list prev = { NULL, NULL, 0 };

    if (node == NULL)
        return;

    if (node->flags & NODE_VISITED) {
        if (!ignore_visit)
            node->visits ++;

        return;
    }

    if (node->prev != NULL)
        sort_visit(node->prev, &prev, 1);

    if (node->kind >= N_MUL) {
        /* handle 2 dependency nodes */
        struct node_list left = { NULL, NULL, 0 }, right = { NULL, NULL, 0 };
        sort_visit(node->deps.op.left, &left, 0);
        sort_visit(node->deps.op.right, &right, 0);

        list->longest_path = sort_3_lists(list, &prev, &left, &right) + 1;
    } else if (node->kind >= N_RETURN) {
        /* handle 1 dependency nodes */
        struct node_list dep = { NULL, NULL, 0 };
        sort_visit(node->deps.single, &dep, 0);

        sort_2_lists(list, &prev, &dep);
        list->longest_path = dep.longest_path + 1;
    } else {
        join_lists(list, &prev);
        list->longest_path = 1;
    }

    node->flags |= NODE_VISITED;
    node->visits = !ignore_visit;

    if (list->tail == NULL)
        list->head = list->tail = node;
    else
        (list->tail->sorted_next = node)->references ++;
    list->tail = node;
}

struct node *topological_sort(struct node *node) {
    struct node_list list = { NULL, NULL, 0 };

    if (node == NULL)
        return NULL;

    node->sorted_next = NULL;
    sort_visit(node, &list, 1);
    return list.head;
}
#endif
