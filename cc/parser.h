#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* an entry in a linked list of scopes. if this scope has no variables declared,
 * `variables` will be set to NULL */
struct scope {
    struct hashtable *variables;
    struct node *last_side_effect;
    struct scope *next;
};

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
};

struct phi_list {
    struct node *last_assignment;
    struct phi_list *next;
};

int parse(struct lex_state *state);

#endif
