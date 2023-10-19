#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "parser.h"
#include "lexer.h"
#include "hashtable.h"
#include "type.h"
#include "output.h"

static const char *expected_paren = "expected `)'";
static const char *expected_type = "expected type signature";
static const char *expected_args = "expected expression or `)'";
static const char *undeclared_identifier = "undeclared identifier";
static const char *expected_expression = "expected expression";
static const char *no_variable_name = "variables must be named";
static const char *duplicate_variable = "duplicate variable name";
static const char *bad_operands = "operand(s) must be arithmetic types";
static const char *bad_lvalue = "assignment left-hand side must be an lvalue";
static const char *expected_statement = "expected statement";
static const char *type_mismatch = "type mismatch";

static int parse_assignment_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
);

static int parse_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
);

static void parse_function_arguments(
    struct lex_state *state,
    struct scope *scope,
    struct token *ident,
    struct written_node *written
) {
    struct token t;
    struct node node;
    const char *name;

    if (!lex(state, &t))
        lex_error(state, expected_args);

    /* TODO: proper argument parsing */

    if (t.kind != T_CLOSE_PAREN)
        lex_error(state, expected_paren);

    node.kind = N_CALL;
    node.references = 1;
    /* TODO: get return type from function declaration */
    node.type = 0;
    node.flags = NODE_IS_ARITH_TYPE;
    node.prev = scope->last_side_effect;

    write_node(&node, written, output_file);

    scope->last_side_effect = written->offset;

    name = token_to_string(state, ident);
    fwrite_checked(name, strlen(name), output_file);
}

/* identifier
 * constant
 * string-literal
 * (expression) */
static int parse_primary_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t, t2;
    struct node node;
    char *name;
    uint32_t hash_value;
    struct scope *current;
    struct variable *variable;
    uint32_t literal;

    if (!lex(state, &t))
        return 0;

    switch (t.kind) {
        case T_IDENT:
            /* check whether function arguments follow and parse them.
             * technically this counts as a postfix expression, not a primary
             * expression, however primary expressions are only used in parsing
             * posfix expressions so it doesn't matter */
            if (lex(state, &t2))
                if (t2.kind == T_OPEN_PAREN) {
                    parse_function_arguments(state, scope, &t, written);
                    return 1;
                } else
                    lex_rewind(state);

            name = token_to_string(state, &t);
            hash_string(name, &hash_value);
            for (current = scope; current != NULL; current = current->next)
                if (
                    current->variables != NULL &&
                    (variable = (struct variable *) hashtable_lookup_hashed(
                        current->variables,
                        name,
                        hash_value
                    )) != NULL
                ) {
                    written->offset = variable->last_assignment;
                    written->variable = variable;
                    variable->references ++;

                    return 1;
                }
            lex_error(state, undeclared_identifier);
        case T_NUMBER:
            node.kind = N_LITERAL;
            node.references = 0;
            node.prev = 0;
            node.type = 0;
            node.flags = NODE_IS_ARITH_TYPE;

            write_node(&node, written, output_file);
            literal = token_to_number(state, &t, 10);
            fwrite_checked(&literal, 4, output_file);

            return 1;
        case T_HEX_NUMBER:
            node.kind = N_LITERAL;
            node.references = 0;
            node.prev = 0;
            node.type = 0;
            node.flags = NODE_IS_ARITH_TYPE;

            write_node(&node, written, output_file);
            literal = token_to_number(state, &t, 16);
            fwrite_checked(&literal, 4, output_file);

            return 1;
        case T_OCT_NUMBER:
            node.kind = N_LITERAL;
            node.references = 0;
            node.prev = 0;
            node.type = 0;
            node.flags = NODE_IS_ARITH_TYPE;

            write_node(&node, written, output_file);
            literal = token_to_number(state, &t, 8);
            fwrite_checked(&literal, 4, output_file);

            return 1;
        case T_OPEN_PAREN:
            /*new = parse_expression(state, scope);
            if (new == NULL)
                lex_error(state, expected_expression);
            if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
                lex_error(state, expected_paren);*/
            fprintf(stderr, "can't parse expression yet\n");
            exit(1);
        default:
            lex_rewind(state);
            return 0;
    }
}

/* written argument contains the child node, and is overwritten with the
 * parent */
static void make_unary_node(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written,
    enum node_kind kind
) {
    struct node parent;

    if (!(written->flags & NODE_IS_ARITH_TYPE))
        lex_error(state, bad_operands);

    parent.kind = kind;
    parent.references = 0;
    parent.prev = 0;
    parent.type = written->type;
    parent.flags = NODE_IS_ARITH_TYPE;
    parent.left = written->offset;
    mod_references(written, 1, output_file);

    /* this has to be last since it overwrites the written_node struct */
    write_node(&parent, written, output_file);
}

/* primary-expression
 * postfix-expression [expression] 
 * postfix-expression (argument-expression-list<opt>) 
 * postfix-expression . identifier
 * postfix-expression -> identifier
 * postfix-expression ++ 
 * postfix-expression -- */
static int parse_postfix_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;

    if (!parse_primary_expression(state, scope, written))
        return 0;

    if (!lex(state, &t))
        return 1;

    switch (t.kind) {
        case T_OPEN_BRACKET:
            lex_error(state, "indexing unimplemented");
        case T_OPEN_PAREN:
            lex_error(state, "function call unimplemented");
        case T_PERIOD:
        case T_ARROW:
            lex_error(state, "field access unimplemented");
        case T_INCREMENT:
            lex_error(state, "increment unimplemented");
        case T_DECREMENT:
            lex_error(state, "decrement unimplemented");
        default:
            lex_rewind(state);
            return 1;
    }
}

static int parse_cast_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
);

static int parse_unary_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
);

/* sizeof unary-expression
 * sizeof ( type-name ) */
static void parse_sizeof(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct node node;
    struct token t;
    uint32_t size = 0;

    if (!lex(state, &t))
        lex_error(state, expected_expression);

    if (t.kind == T_OPEN_PAREN) {
        struct type *type;
        char *unused;

        if (!parse_type_signature(state, &type, &unused))
            lex_error(state, expected_type);

        size = type->size;

        free_type(type);
    } else {
        struct written_node temp;

        lex_rewind(state);

        /*if (!parse_unary_expression(state, scope, &temp))
            lex_error(state, expected_expression);*/

        /*if (temp->type != NULL)
            size = temp->type->size;*/
        lex_error(state, "type reading unimplemented");
    }

    node.kind = N_LITERAL;
    node.references = 0;
    node.prev = 0;
    node.type = 0;
    node.flags = NODE_IS_ARITH_TYPE;

    write_node(&node, written, output_file);
    fwrite_checked(&size, 4, output_file);
}

/* postfix-expression
 * ++ unary-expression
 * -- unary-expression
 * unary-operator cast-expression
 * sizeof unary-expression
 * sizeof(type-name) */
static int parse_unary_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;

    if (!lex(state, &t))
        return 0;

    switch (t.kind) {
        case T_INCREMENT:
            /*return make_unary_node(
                state,
                scope,
                parse_unary_expression(state, scope),
                N_PRE_INC
            );*/
            lex_error(state, "pre increment unimplemented");
        case T_DECREMENT:
            /*return make_unary_node(
                state,
                scope,
                parse_unary_expression(state, scope),
                N_PRE_DEC
            );*/
            lex_error(state, "pre decrement unimplemented");
        case T_AMPERSAND:
            /* TODO: handle reference operator */
            lex_error(state, "reference unimplemented");
        case T_ASTERISK:
            if (!parse_cast_expression(state, scope, written))
                lex_error(state, expected_expression);

            make_unary_node(state, scope, written, N_DEREF);

            written->flags |= NODE_IS_LVALUE;

            return 1;
        case T_PLUS:
            if (!parse_cast_expression(state, scope, written))
                lex_error(state, expected_expression);

            if (!(written->flags & NODE_IS_ARITH_TYPE))
                lex_error(state, bad_operands);

            return 1;
        case T_MINUS:
            if (!parse_cast_expression(state, scope, written))
                lex_error(state, expected_expression);

            make_unary_node(state, scope, written, N_NEGATE);

            return 1;
        case T_BITWISE_NOT:
            if (!parse_cast_expression(state, scope, written))
                lex_error(state, expected_expression);

            make_unary_node(state, scope, written, N_BITWISE_NOT);

            return 1;
        case T_LOGICAL_NOT:
            if (!parse_cast_expression(state, scope, written))
                lex_error(state, expected_expression);

            make_unary_node(state, scope, written, N_NOT);

            return 1;
        case T_SIZEOF:
            parse_sizeof(state, scope, written);
            return 1;
        default:
            lex_rewind(state);
            return parse_postfix_expression(state, scope, written);
    }
}

/* unary-expression
 * (type-name) cast-expression */
static int parse_cast_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    /* TODO: handle casting */
    return parse_unary_expression(state, scope, written);
}

/* propagate the given type to the given node and its dependencies */
static void propagate_type(
    off_t offset,
    off_t type,
    size_t size,
    FILE *stream
) {
    struct node node;

    for (; offset != 0; offset = node.left) {
        read_node(offset, &node, stream);

        if (node.type != 0)
            break;

        fseek(output_file, offset + offsetof(struct node, type), SEEK_SET);
        fwrite_checked(&type, sizeof(size_t), stream);

        if (node.kind >= N_MUL)
            /* handle nodes with 2 dependencies */
            propagate_type(node.right, type, size, stream);
    }
}

static void lhs_rhs_to_node(
    struct lex_state *state,
    struct written_node *lhs,
    struct written_node *rhs,
    struct written_node *written,
    enum node_kind kind
) {
    struct node new;
    struct type lhs_type, rhs_type;

    if (
        !(lhs->flags & NODE_IS_ARITH_TYPE)
        || !(rhs->flags & NODE_IS_ARITH_TYPE)
    )
        lex_error(state, bad_operands);

    mod_references(lhs, 1, output_file);
    mod_references(rhs, 1, output_file);

    new.kind = kind;
    new.references = 0;
    new.prev = 0;
    new.type = 0;

    /* figure out what type this node should have. if one side has an explicit
     * type and the other doesn't, then this node takes the explicit type if
     * it's larger than an int. if both sides have types, the type with the
     * largest size is used */
    if (lhs->type != 0 && rhs->type == 0) {
        off_t offset = ftell(output_file);

        fseek(output_file, lhs->type, SEEK_SET);
        read_type_no_data(&lhs_type, output_file);

        if (lhs_type.size > 2) /* TODO: int size abstraction */
            propagate_type(rhs->offset, lhs->type, lhs_type.size, output_file);

        fseek(output_file, offset, SEEK_SET);
    } else if (lhs->type == 0 && rhs->type != 0) {
        off_t offset = ftell(output_file);

        fseek(output_file, rhs->type, SEEK_SET);
        read_type_no_data(&rhs_type, output_file);

        if (rhs_type.size > 2)
            propagate_type(lhs->offset, rhs->type, rhs_type.size, output_file);

        fseek(output_file, offset, SEEK_SET);
    } else if (lhs->type != 0) {
        off_t offset = ftell(output_file);

        fseek(output_file, lhs->type, SEEK_SET);
        read_type_no_data(&lhs_type, output_file);

        fseek(output_file, rhs->type, SEEK_SET);
        read_type_no_data(&rhs_type, output_file);

        fseek(output_file, offset, SEEK_SET);

        if (lhs_type.size > rhs_type.size && lhs_type.size > 2)
            new.type = lhs->type;
        else if (rhs_type.size > 2)
            new.type = rhs->type;
    }

    new.flags = NODE_IS_ARITH_TYPE;
    new.left = lhs->offset;
    new.right = rhs->offset;

    /* this must be last in case written points to lhs or rhs */
    write_node(&new, written, output_file);
}

/* cast-expression
 * multiplicative-expression * cast-expression
 * multiplicative-expression / cast-expression
 * multiplicative-expression % cast-expression */
static int parse_mul_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_cast_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        switch (t.kind) {
            case T_ASTERISK:
                if (!parse_cast_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_MUL);
                break;
            case T_DIVIDE:
                if (!parse_cast_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_DIV);
                break;
            case T_MODULO:
                if (!parse_cast_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_MOD);
                break;
            default:
                lex_rewind(state);
                return 1;
        }
    }
}

/* multiplicative-expression
 * additive-expression + multiplicative-expression
 * additive-expression - multiplicative-expression */
static int parse_add_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_mul_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        switch (t.kind) {
            case T_PLUS:
                if (!parse_mul_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_ADD);
                break;
            case T_MINUS:
                if (!parse_mul_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_SUB);
                break;
            default:
                lex_rewind(state);
                return 1;
        }
    }
}

/* additive-expression
 * shift-expression << additive-expression
 * shift-expression >> additive-expression */
static int parse_shift_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_add_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        switch (t.kind) {
            case T_LEFT_SHIFT:
                if (!parse_add_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_SHIFT_LEFT);
                break;
            case T_RIGHT_SHIFT:
                if (!parse_add_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_SHIFT_RIGHT);
                break;
            default:
                lex_rewind(state);
                return 1;
        }
    }
}

/* shift-expression
 * relational-expression < shift-expression
 * relational-expression > shift-expression
 * relational-expression <= shift-expression
 * relational-expression >= shift-expression */
static int parse_rel_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_shift_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        switch (t.kind) {
            case T_LESS_THAN:
                if (!parse_shift_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_LESS);
                break;
            case T_GREATER_THAN:
                if (!parse_shift_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_GREATER);
                break;
            case T_LESS_EQ:
                if (!parse_shift_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_LESS_EQ);
                break;
            case T_GREATER_EQ:
                if (!parse_shift_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_GREATER_EQ);
                break;
            default:
                lex_rewind(state);
                return 1;
        }
    }
}

/* relational-expression
 * equality-expression == relational-expression
 * equality-expression != relational-expression */
static int parse_eq_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_rel_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        switch (t.kind) {
            case T_EQUALS:
                if (!parse_rel_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_EQUALS);
                break;
            case T_NOT_EQ:
                if (!parse_rel_expression(state, scope, &rhs))
                    lex_error(state, expected_expression);

                lhs_rhs_to_node(state, written, &rhs, written, N_NOT_EQ);
                break;
            default:
                lex_rewind(state);
                return 1;
        }
    }
}

/* equality-expression
 * AND-expression & equality-expression */
static int parse_bitwise_and_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_eq_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        if (t.kind == T_AMPERSAND) {
            if (!parse_eq_expression(state, scope, &rhs))
                lex_error(state, expected_expression);

            lhs_rhs_to_node(state, written, &rhs, written, N_BITWISE_AND);
        } else {
            lex_rewind(state);
            return 1;
        }
    }
}

/* AND-expression
 * exclusive-OR-expression ^ AND-expression */
static int parse_bitwise_xor_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_bitwise_and_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        if (t.kind == T_BITWISE_XOR) {
            if (!parse_bitwise_and_expression(state, scope, &rhs))
                lex_error(state, expected_expression);

            lhs_rhs_to_node(state, written, &rhs, written, N_BITWISE_XOR);
        } else {
            lex_rewind(state);
            return 1;
        }
    }
}

/* exclusive-OR-expression
 * inclusive-OR-expression | exclusive-OR-expression */
static int parse_bitwise_or_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_bitwise_xor_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        if (t.kind == T_BITWISE_OR) {
            if (!parse_bitwise_xor_expression(state, scope, &rhs))
                lex_error(state, expected_expression);

            lhs_rhs_to_node(state, written, &rhs, written, N_BITWISE_XOR);
        } else {
            lex_rewind(state);
            return 1;
        }
    }
}

/* inclusive-OR-expression
 * logical-AND-expression && inclusive-OR-expression */
static int parse_logical_and_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_bitwise_or_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        if (t.kind == T_LOGICAL_AND) {
            if (!parse_bitwise_or_expression(state, scope, &rhs))
                lex_error(state, expected_expression);

            lhs_rhs_to_node(state, written, &rhs, written, N_AND);
        } else {
            lex_rewind(state);
            return 1;
        }
    }
}

/* logical-AND-expression
 * logical-OR-expression || logical-AND-expression */
static int parse_logical_or_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;
    struct written_node rhs;

    if (!parse_logical_and_expression(state, scope, written))
        return 0;

    while (1) {
        if (!lex(state, &t))
            return 1;

        if (t.kind == T_LOGICAL_OR) {
            if (!parse_logical_and_expression(state, scope, &rhs))
                lex_error(state, expected_expression);

            lhs_rhs_to_node(state, written, &rhs, written, N_OR);
        } else {
            lex_rewind(state);
            return 1;
        }
    }
}

/* logical-OR-expression
 * logical-OR-expression ? expression : conditional-expression */
static int parse_conditional_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    /* TODO */
    return parse_logical_or_expression(state, scope, written);
}

static unsigned char are_types_compatible(struct type *a, off_t b) {
    /* TODO */
    return 0;
}

static void parse_variable_assignment(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written,
    struct variable *variable
) {
    if (!parse_assignment_expression(state, scope, written))
        lex_error(state, expected_expression);

    written->flags |= NODE_IS_LVALUE;

    if (written->type == 0 && variable->type->size > 2) {
        /* if the new value doesn't have an associated type, it gains the type
         * of the variable */

        if (variable->written_type == 0)
            variable->written_type = write_type(variable->type, output_file);

        propagate_type(
            written->type,
            variable->written_type,
            variable->type->size,
            output_file
        );
    } else if (
        ((variable->flags & VAR_IS_ARITH_TYPE) > 0)
        != ((written->flags & NODE_IS_ARITH_TYPE) > 0)
    )
        /* ensure both sides are or are not arithmetic types */
        lex_error(state, type_mismatch);
    else if (
        !(written->flags & NODE_IS_ARITH_TYPE)
        && !are_types_compatible(variable->type, written->type)
    )
        /* ensure both non-arithmetic types are compatible */
        lex_error(state, type_mismatch);

    variable->last_assignment = written->offset;
}

/* conditional-expression
 * unary-expression assignment-operator assignment-expression
 * (this parser doesn't allow parsing strictly a unary expression, so anything
 * that produces an lvalue is accepted) */
static int parse_assignment_expression(
    struct lex_state *state,
    struct scope *scope,
    struct written_node *written
) {
    struct token t;

    if (!parse_conditional_expression(state, scope, written))
        return 0;

    if (!lex(state, &t))
        return 1;

    if (t.kind == T_ASSIGN) {
        lex_rewind(state);
        return 1;
    }

    if (!(written->flags & NODE_IS_LVALUE))
        lex_error(state, bad_lvalue);

    if (written->variable != NULL) {
        /* parse assignment to variable */
        parse_variable_assignment(
            state,
            scope,
            written,
            written->variable
        );
        return 1;
    }

    /* parse assignment to dereferenced value */
    struct written_node rhs;
    struct node node;

    if (!parse_assignment_expression(state, scope, &rhs))
        lex_error(state, expected_expression);

    node.kind = N_ASSIGN;
    node.references = 1;
    node.type = rhs.type;
    node.flags = (rhs.flags & NODE_IS_ARITH_TYPE);
    node.prev = scope->last_side_effect;

    mod_references(written, 1, output_file);
    node.right = written->offset;

    mod_references(&rhs, 1, output_file);
    node.right = rhs.offset;

    write_node(&node, written, output_file);
    scope->last_side_effect = written->offset;

    return 1;
}

#if 0
/* assignment-expression
 * expression , assignment-expression */
static struct node *parse_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct token t;
    char has_looped = 0;

    while (1) {
        struct node *new = parse_assignment_expression(state, scope);

        if (new == NULL)
            if (has_looped)
                lex_error(state, expected_expression);
            else
                return NULL;

        if (!lex(state, &t))
            return new;

        if (t.kind == T_COMMA) {
            if (new->flags & NODE_HAS_SIDE_EFFECTS) {
                new->prev = scope->last_side_effect;

                scope->last_side_effect = new;
                new->references ++;
            } else
                free_node(new);
        } else {
            lex_rewind(state);
            return new;
        }

        has_looped |= 1;
    }
}

static char parse_declaration(struct lex_state *state, struct scope *scope) {
    struct type *basic_type, *type;
    char *name;
    struct token t;

    basic_type = (struct type *) malloc(sizeof(struct type));
    if (basic_type == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    if (!parse_basic_type(state, basic_type)) {
        free(basic_type);
        return 0;
    }

    for (
        name = NULL, type = basic_type;
        parse_type_derivations(state, &type, &name);
    ) {
        struct variable *variable;

        if (name == NULL)
            lex_error(state, no_variable_name);

        if (type->size == 0)
            lex_error(state, unknown_size);

        if (scope->variables == NULL) {
            scope->variables = (struct hashtable *)
                malloc(sizeof(struct hashtable));
            if (scope->variables == NULL) {
                perror(malloc_failed);
                exit(1);
            }
            hashtable_init(scope->variables);
        }

        variable = (struct variable *) malloc(sizeof(struct variable));
        if (variable == NULL) {
            perror(malloc_failed);
            exit(1);
        }

        variable->type = type;
        variable->last_assignment = NULL;
        variable->phi_list = NULL;
        variable->parent = NULL;
        variable->references = 1;
        variable->flags = 0;
        if (
            (
                type->top == TOP_BASIC &&
                type->type.basic.type_specifier <= TY_LONG_LONG
            ) || type->top == TOP_POINTER
        )
            variable->flags |= VAR_IS_ARITH_TYPE;

        if (!hashtable_insert(scope->variables, name, variable))
            lex_error(state, duplicate_variable);

        if (!lex(state, &t))
            lex_error(state, expected_semicolon);

        if (t.kind == T_ASSIGN) {
            parse_variable_assignment(state, scope, variable);

            if (!lex(state, &t))
                lex_error(state, expected_semicolon);
        } else {
            struct node *new = (struct node *) malloc(sizeof(struct node));
            if (new == NULL) {
                perror(malloc_failed);
                exit(1);
            }

            new->kind = N_LVALUE;
            new->deps.single = NULL;
            new->prev = NULL;
            new->data.lvalue = variable;
            variable->references ++;
            new->type = variable->type;
            new->references = 1; /* single reference is last_assignment */
            new->flags = NODE_IS_LVALUE;
            if (variable->flags & VAR_IS_ARITH_TYPE)
                new->flags |= NODE_IS_ARITH_TYPE;

            variable->last_assignment = new;
        }

        switch (t.kind) {
            case T_COMMA:
                continue;
            case T_SEMICOLON:
                break;
            default:
                lex_error(state, expected_semicolon);
        }

        break;
    }

    return 1;
}

void free_variable_hashtable(struct variable *variable) {
    variable->references --;

    if (variable->parent != NULL && variable->last_assignment != NULL) {
        struct phi_list *new = (struct phi_list *)
            malloc(sizeof(struct phi_list));
        if (new == NULL) {
            perror(malloc_failed);
            exit(1);
        }

        (new->last_assignment = variable->last_assignment)->references ++;
        new->next = variable->parent->phi_list;
        variable->parent->phi_list = new;
    }

    free_variable(variable);
}

static struct node *parse_statement(
    struct lex_state *state,
    struct scope *scope
);

static struct node *parse_compound(
    struct lex_state *state,
    struct scope *scope
) {
    struct token t;
    struct scope new_scope = {
        .variables = NULL,
        .last_side_effect = NULL,
        .next = scope
    };

    /* parse declaration list */
    while (parse_declaration(state, &new_scope));

    /* parse statements */
    while (1) {
        struct node *new;

        if (!lex(state, &t))
            break;

        switch (t.kind) {
            case T_SEMICOLON:
                continue;
            case T_CLOSE_CURLY:
                break;
            default:
                lex_rewind(state);

                new = parse_statement(state, &new_scope);
                if (new == NULL)
                    lex_error(state, expected_statement);

                if (new->flags & NODE_HAS_SIDE_EFFECTS) {
                    new->prev = new_scope.last_side_effect;

                    new_scope.last_side_effect = new;
                    new->references ++;
                } else
                    free_node(new);

                continue;
        }

        break;
    }

    /* free the variables hashtable */
    if (new_scope.variables != NULL) {
        hashtable_free(
            new_scope.variables,
            (void (*)(void *)) &free_variable_hashtable
        );
        free(new_scope.variables);
    }

    return new_scope.last_side_effect;
}

static struct node *parse_expression_statement(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *new = parse_expression(state, scope);
    struct token t;

    /* require trailing semicolon if an expression was parsed */
    if (new != NULL)
        if (!lex(state, &t) || t.kind != T_SEMICOLON)
            lex_error(state, expected_semicolon);

    /* skip any extra trailing semicolons */
    while (lex(state, &t))
        if (t.kind != T_SEMICOLON) {
            lex_rewind(state);
            break;
        }

    return new;
}

static struct node *parse_statement(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *new;
    struct token t;

    if (!lex(state, &t))
        return NULL;

    switch (t.kind) {
        /* compound-statement */
        case T_OPEN_CURLY:
            return parse_compound(state, scope);
        /* labeled-statement*/
        case T_CONTINUE:
        case T_BREAK:
            lex_error(state, "labeled statements unimplemented");
        case T_RETURN:
            new = (struct node *) malloc(sizeof(struct node));
            if (new == NULL) {
                perror(malloc_failed);
                exit(1);
            }

            new->kind = N_RETURN;
            new->deps.single = parse_expression(state, scope);
            if (new->deps.single != NULL)
                new->deps.single->references ++;
            new->prev = NULL;
            new->type = NULL;
            new->references = 0;
            new->flags = NODE_HAS_SIDE_EFFECTS;

            if (!lex(state, &t) || t.kind != T_SEMICOLON)
                lex_error(state, expected_semicolon);

            return new;
        default:
            lex_rewind(state);
    }

    /* expression-statement */
    return parse_expression_statement(state, scope);
}
#endif

int parse(struct lex_state *state) {
    struct written_node written;

    /*debug_sorted(topological_sort(parse_statement(state, NULL)));*/
    parse_primary_expression(state, NULL, &written);

    return 0;
}
