#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "parser.h"
#include "lexer.h"
#include "hashtable.h"

/* error messages */
const char *malloc_failed = "failed to allocate memory";
const char *dup_type_spec_err = "duplicate type specifier";
const char *dup_storage_err = "duplicate storage class";
const char *dup_sign_err = "duplicate sign specifier";
const char *dup_type_qual_err = "duplicate type qualifier";
const char *no_type_spec = "expected type specifier";
const char *missing_name_def = "expected identifier or `{'";
const char *bad_array_syntax = "expected number or `]'";
const char *unexpected_qual = "unexpected type qualifier";
const char *unfinished_struct_union_def = "expected type or `}'";
const char *expected_semicolon = "expected `;'";
const char *duplicate_field = "field with this name was already declared";
const char *unknown_size = "storage size of type is unknown";
const char *field_missing_name = "field missing name";
const char *expected_paren = "expected `)'";
const char *expected_comma_or_paren = "expected `,' or `)'";
const char *expected_type = "expected type signature";
const char *unexpected_eof = "unexpected EOF";

/* given a token, read the area in the file it points to into a newly allocated
 * null terminated string and return it */
static char *token_to_string(struct lex_state *state, struct token *t) {
    char *string;
    unsigned int len, len2, current;

    len = t->file_end - t->file_start;
    string = (char *) malloc(len + 1);
    if (string == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    current = ftell(state->stream);
    fseek(state->stream, t->file_start, SEEK_SET);
    if ((len2 = fread(string, 1, len, state->stream)) != len) {
        free(string);
        return NULL;
    }
    string[len] = 0;
    fseek(state->stream, current, SEEK_SET);

    return string;
}

/* given a token, read the area in the file it points to and parse it into a
 * number of the given base, up to base 16 */
/* TODO: add binary notation (0b...) support since it'll be easy */
static unsigned long token_to_number(
    struct lex_state *state,
    struct token *t,
    unsigned long base
) {
    unsigned long result = 0;
    unsigned int len, i = 0;
    char c;

    len = t->file_end - t->file_start;

    fseek(state->stream, t->file_start, SEEK_SET);

    for (; i < len; i ++) {
        if (!fread(&c, 1, 1, state->stream))
            lex_error(state, "what the fuck");

        result *= base;

        if (isdigit(c))
            result += c - '0';
        else if (c >= 'a' && c <= 'f')
            result += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            result += c - 'A' + 10;
    }

    return result;
}

static struct struct_union_field *parse_struct_union(struct lex_state *state);

/* macros to deduplicate wacky code in parse_basic_type() */

/* set the type specifier to the given type specifier, set the
 * have_type_specifier flag, and throw an error if a type specifier has already
 * been set */
#define SET_TYPE_SPECIFIER(t) {\
    if (have_type_specifier)\
        lex_error(state, dup_type_spec_err);\
    have_type_specifier = 1;\
    type_specifier = t;\
}
/* same as SET_TYPE_SPECIFIER() but with storage class. since there's a default
 * otherwise unobtainable value for storage class we don't need to set any flags
 */
#define SET_STORAGE_CLASS(c) {\
    if (storage_class != C_AUTO)\
        lex_error(state, dup_storage_err);\
    storage_class = c;\
}
/* same as SET_STORAGE_CLASS() but for sign specifier */
#define SET_SIGN_SPECIFIER(s) {\
    if (sign_specifier != S_UNKNOWN)\
        lex_error(state, dup_sign_err);\
    sign_specifier = s;\
}
/* parse struct name and/or fields */
#define PARSE_STRUCT_UNION(s) {\
    if (have_type_specifier)\
        lex_error(state, dup_type_spec_err);\
    if (!lex(state, &t))\
        lex_error(state, missing_name_def);\
    switch (t.kind) {\
        case T_IDENT:\
            if (lex(state, &t2))\
                if (t2.kind != T_OPEN_CURLY)\
                    lex_rewind(state);\
                else {\
                    /* TODO: store name and fields somewhere */\
                    first_field = parse_struct_union(state);\
                    have_fields = 1;\
                    break;\
                }\
            name = token_to_string(state, &t);\
            have_fields = 0;\
            break;\
        case T_OPEN_CURLY:\
            first_field = parse_struct_union(state);\
            have_fields = 1;\
            break;\
        default:\
            lex_error(state, missing_name_def);\
            break;\
    }\
    have_type_specifier = 1;\
    type_specifier = s;\
}

/* parses a "basic type" signature (i.e. anything in enum type_specifier)
 * and its storage class, sign specifier, and qualifiers. returns 1 on success,
 * 0 on failure. contents of the type pointer will only be modified on success
 */
static char parse_basic_type(struct lex_state *state, struct type *type) {
    struct token t, t2;
    char *name = NULL;
    enum type_specifier type_specifier;
    unsigned char have_type_specifier = 0;
    enum storage_class storage_class = C_AUTO;
    enum sign_specifier sign_specifier = S_UNKNOWN;
    unsigned char have_fields = 0;
    unsigned char const_qualified = 0;
    unsigned char volatile_qualified = 0;
    struct struct_union_field *first_field = NULL;
    unsigned int size = 0;

    while (1) {
        if (!lex(state, &t))
            break;

        switch (t.kind) {
            case T_VOID:
                SET_TYPE_SPECIFIER(TY_VOID);
                size = 0;
                continue;
            case T_CHAR:
                SET_TYPE_SPECIFIER(TY_CHAR);
                size = 1;
                continue;
            case T_SHORT:
                SET_TYPE_SPECIFIER(TY_SHORT);
                size = 2;
                continue;
            case T_LONG:
                if (have_type_specifier)
                    if (type_specifier == TY_LONG) {
                        type_specifier = TY_LONG_LONG;
                        size = 8;
                    } else
                        lex_error(state, dup_type_spec_err);

                have_type_specifier = 1;
                type_specifier = TY_LONG;
                size = 4;
                continue;
            case T_INT:
                if (
                    have_type_specifier && type_specifier != TY_SHORT &&
                    type_specifier != TY_LONG && type_specifier != TY_LONG_LONG
                )
                    lex_error(state, dup_type_spec_err);

                have_type_specifier = 1;
                type_specifier = TY_INT;
                size = 2;
                continue;
            case T_EXTERN:
                SET_STORAGE_CLASS(C_EXTERN);
                continue;
            case T_STATIC:
                SET_STORAGE_CLASS(C_STATIC);
                continue;
            case T_UNSIGNED:
                SET_SIGN_SPECIFIER(S_UNSIGNED);
                continue;
            case T_SIGNED:
                SET_SIGN_SPECIFIER(S_SIGNED);
                continue;
            case T_CONST:
                if (const_qualified)
                    lex_error(state, dup_type_qual_err);
                else
                    const_qualified = 1;
                continue;
            case T_VOLATILE:
                if (volatile_qualified)
                    lex_error(state, dup_type_qual_err);
                else
                    volatile_qualified = 1;
                continue;
            case T_STRUCT:
                PARSE_STRUCT_UNION(TY_STRUCT);
                size = 0;
                if (have_fields && first_field != NULL) {
                    struct struct_union_field *field = first_field;

                    while (1)
                        if (field->next == NULL) {
                            size = field->offset + field->type->size;
                            break;
                        } else
                            field = field->next;
                }
                continue;
            case T_UNION:
                PARSE_STRUCT_UNION(TY_UNION);
                size = 0;
                if (have_fields && first_field != NULL) {
                    struct struct_union_field *field = first_field;

                    while (field != NULL) {
                        if (field->type->size > size)
                            size = field->type->size;

                        field = field->next;
                    }
                }
                continue;
            default:
                /* rewind the lexer to pass this token back to whatever called
                 * this function */
                lex_rewind(state);
                break;
        }

        break;
    }

    if (!have_type_specifier)
        if (
            storage_class == C_AUTO && sign_specifier == S_UNKNOWN &&
            !const_qualified && !volatile_qualified
        )
            return 0;
        else
            lex_error(state, no_type_spec);

    type->top = TOP_BASIC;
    type->type.basic.type_specifier = type_specifier;
    type->type.basic.storage_class = storage_class;
    type->type.basic.sign_specifier = sign_specifier;
    type->type.basic.const_qualified = const_qualified;
    type->type.basic.volatile_qualified =
        volatile_qualified;
    if (type->type.basic.has_fields = have_fields)
        type->type.basic.name_field_ptr = first_field;
    else
        type->type.basic.name_field_ptr = name;
    type->size = size;
    type->references = 0;

    return 1;
}

/* duplicates a type, safely allocating memory for it in the process */
static struct type *dup_type(struct type *type) {
    struct type *new;
    new = (struct type *) malloc(sizeof(struct type));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }
    *new = *type;
    return new;
}

static void derive_type(
    struct type **first_derivation,
    struct type **last_derivation
) {
    /* allocate memory for a new type */
    struct type *new;
    new = (struct type *) malloc(sizeof(struct type));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }
    new->references = 0;

    if (*last_derivation == NULL) {
        /* no types have been derived, set first_derivation to the newly
         * allocated type so that it'll point to the start of the derivation
         * list */
        *last_derivation = *first_derivation = new;
        new = NULL;
    } else {
        /* there's already a derivation list, just add on to the end */
        struct type *temp = new;
        new = *last_derivation;
        *last_derivation = temp;
    }

    (*last_derivation)->type.derived.derivation = new;
    if (new != NULL)
        new->references ++;
}

static void derive_reverse(
    struct type **first_derivation,
    struct type **last_derivation
) {
    struct type *new;
    new = (struct type *) malloc(sizeof(struct type));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    if (*first_derivation == NULL) {
        *first_derivation = *last_derivation = new;
        new->references = 0;
    } else {
        (*first_derivation)->type.derived.derivation = new;
        new->references = 1;
        *first_derivation = new;
    }
}

static void parse_array_internal(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation,
    unsigned long base
) {
    struct token t;
    derive_reverse(first_derivation, last_derivation);
    (*first_derivation)->top = TOP_ARRAY;
    lex(state, &t);
    (*first_derivation)->size = -1;
    /*    (*first_derivation)->type.derived.derivation->size * */
    (*first_derivation)->type.derived.type.array.length =
        token_to_number(state, &t, base);
    if (!lex(state, &t) || t.kind != T_CLOSE_BRACKET)
        lex_error(state, bad_array_syntax);
}

static void parse_array(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
) {
    struct token t;
    struct type *first_array = NULL;
    struct type *last_array = NULL;

    while (1) {
        if (!lex(state, &t))
            lex_error(state, bad_array_syntax);

        switch (t.kind) {
            case T_NUMBER:
                lex_rewind(state);
                parse_array_internal(state, &first_array, &last_array, 10);
                break;
            case T_HEX_NUMBER:
                lex_rewind(state);
                parse_array_internal(state, &first_array, &last_array, 16);
                break;
            case T_OCT_NUMBER:
                lex_rewind(state);
                parse_array_internal(state, &first_array, &last_array, 8);
                break;
            case T_CLOSE_BRACKET:
                derive_reverse(&first_array, &last_array);
                first_array->top = TOP_ARRAY;
                first_array->size = 0;
                first_array->type.derived.type.array.length = 0;
                break;
            default:
                lex_error(state, bad_array_syntax);
        }

        if (!lex(state, &t))
            break;

        if (t.kind != T_OPEN_BRACKET) {
            lex_rewind(state);
            break;
        }
    }

    if (first_array != NULL) {
        /* add array derivations to the type */
        first_array->type.derived.derivation = *last_derivation;
        *last_derivation = last_array;
        if (*first_derivation == NULL)
            *first_derivation = first_array;
    }
}

static void parse_function_type_arguments(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
);

static void parse_type_after_name(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
) {
    struct token t;

    if (!lex(state, &t))
        return;

    switch (t.kind) {
        case T_OPEN_BRACKET:
            parse_array(state, first_derivation, last_derivation);
            break;
        case T_OPEN_PAREN:
            parse_function_type_arguments(state, first_derivation, last_derivation);
            break;
        default:
            lex_rewind(state);
            break;
    }
}

static char parse_type_derivation(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation,
    char **name
) {
    struct token t;
    struct pointer_type *p;
    struct type *recursive_first = NULL;
    struct type *recursive_last = NULL;

    while (1) {
        if (!lex(state, &t))
            return 0;

        switch (t.kind) {
            case T_ASTERISK:
                derive_type(first_derivation, last_derivation);
                (*last_derivation)->top = TOP_POINTER;
                (*last_derivation)->size = 2;
                /* this is so silly */
                p = &(*last_derivation)->type.derived.type.pointer;
                p->const_qualified = 0;
                p->volatile_qualified = 0;
                break;
            case T_CONST:
                if (
                    (*last_derivation) == NULL ||
                    (*last_derivation)->top != TOP_POINTER
                )
                    lex_error(state, unexpected_qual);

                if (p->const_qualified)
                    lex_error(state, dup_type_qual_err);
                else
                    p->const_qualified = 1;

                break;
            case T_VOLATILE:
                if (
                    (*last_derivation) == NULL ||
                    (*last_derivation)->top != TOP_POINTER
                )
                    lex_error(state, unexpected_qual);

                if (p->volatile_qualified)
                    lex_error(state, dup_type_qual_err);
                else
                    p->volatile_qualified = 1;

                break;
            case T_OPEN_BRACKET:
                parse_array(state, first_derivation, last_derivation);
                return 1;
            case T_OPEN_PAREN:
                if (!lex(state, &t))
                    lex_error(state, unexpected_eof);

                if (
                    t.kind == T_IDENT || t.kind == T_ASTERISK ||
                    t.kind == T_OPEN_BRACKET
                ) {
                    lex_rewind(state);
                    parse_type_derivation(
                        state,
                        &recursive_first,
                        &recursive_last,
                        name
                    );

                    if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
                        lex_error(state, expected_paren);
                } else {
                    lex_rewind(state);
                    parse_function_type_arguments(
                        state,
                        first_derivation,
                        last_derivation
                    );
                }

                parse_type_after_name(state, first_derivation, last_derivation);

                if (recursive_last != NULL) {
                    if (*last_derivation == NULL)
                        *first_derivation = recursive_first;
                    else
                        recursive_first->type.derived.derivation =
                            *last_derivation;

                    *last_derivation = recursive_last;
                }

                return 1;
            case T_IDENT:
                *name = token_to_string(state, &t);

                parse_type_after_name(state, first_derivation, last_derivation);

                return 1;
            default:
                lex_rewind(state);
                return 0;
        }

    }
}

/* parses the name and derivations of an existing basic type */
static char parse_type_derivations(
    struct lex_state *state,
    struct type **type,
    char **name
) {
    struct type *first_derivation = NULL;
    struct type *last_derivation = NULL;
    struct type *current;

    if (!parse_type_derivation(
        state,
        &first_derivation,
        &last_derivation,
        name
    ))
        return 0;

    /* merge the basic type into the derivation list */
    if (first_derivation != NULL) {
        struct type *temp = *type;
        *type = last_derivation;
        first_derivation->type.derived.derivation = temp;

        /* compute all the array sizes in a derivation list. this silly
         * algorithm is like O(n) best case and O(n^2)? worst case so it's a
         * good thing arrays don't get nested often */
        while (1) {
            int found = 0;

            current = *type;

            while (current) {
                struct type *derivation;

                if (
                    current->top == TOP_BASIC ||
                    (derivation = current->type.derived.derivation) == NULL
                )
                    break;

                if (
                    current->top == TOP_ARRAY && current->size == -1 &&
                    derivation->size != -1
                ) {
                    found ++;
                    current->size = derivation->size *
                        current->type.derived.type.array.length;
                }

                current = derivation;
            }

            if (found == 0)
                break;
        }
    }

    return 1;
}

/* parse a type signature. this code is very wacky, just like c type signatures
 * TODO: explain type signature syntax
 */
static char parse_type_signature(
    struct lex_state *state,
    struct type **type,
    char **name
) {
    *type = (struct type *) malloc(sizeof(struct type));

    if (*type == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    *name = NULL;

    if (!parse_basic_type(state, *type))
        return 0;

    return parse_type_derivations(state, type, name);
}

/* parse a struct or union definition */
static struct struct_union_field *parse_struct_union(struct lex_state *state) {
    struct struct_union_field *first = NULL;
    unsigned int offset = 0;

    while (1) {
        char *name;
        struct type *type;
        struct token t;

        if (parse_type_signature(state, &type, &name)) {
            unsigned long hash_value;

            if (name == NULL)
                lex_error(state, field_missing_name);

            if (type->size == 0)
                lex_error(state, unknown_size);

            if (!lex(state, &t) || t.kind != T_SEMICOLON)
                lex_error(state, expected_semicolon);

            hash_string(name, &hash_value);

            if (first == NULL) {
                first = (struct struct_union_field *)
                    malloc(sizeof(struct struct_union_field));
                if (first == NULL) {
                    perror(malloc_failed);
                    exit(1);
                }
                first->type = type;
                first->name = name;
                first->name_hash = hash_value;
                first->offset = offset;
                first->next = NULL;
            } else {
                struct struct_union_field *cur = first;

                /* walking the entire list to check hash values probably isn't
                 * the best idea but it should work fine */
                while (1) {
                    if (cur->name_hash == hash_value && strcmp(name, cur->name)
                        == 0)
                        lex_error(state, duplicate_field);
                    else if (cur->next == NULL) {
                        struct struct_union_field *field =
                            (struct struct_union_field *)
                            malloc(sizeof(struct struct_union_field));
                        if (first == NULL) {
                            perror(malloc_failed);
                            exit(1);
                        }
                        field->type = type;
                        field->name = name;
                        field->name_hash = hash_value;
                        field->offset = offset;
                        field->next = NULL;
                        cur->next = field;
                        break;
                    }
                    cur = cur->next;
                }
            }

            offset += type->size;
        } else if (!lex(state, &t) || t.kind != T_CLOSE_CURLY)
            lex_error(state, unfinished_struct_union_def);
        else
            break;
    }

    return first;
}

static void parse_function_type_arguments(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
) {
    struct token t;
    struct type *type;
    char *name;
    struct function_type *first = NULL;
    struct function_type *last = NULL;

    derive_type(first_derivation, last_derivation);
    (*last_derivation)->top = TOP_FUNCTION;
    (*last_derivation)->size = 0;
    (*last_derivation)->type.derived.type.function = NULL;

    if (!lex(state, &t))
        lex_error(state, expected_type);

    if (t.kind == T_CLOSE_PAREN)
        return;
    else
        lex_rewind(state);

    while (1) {
        struct function_type *new;

        if (!parse_type_signature(state, &type, &name))
            lex_error(state, expected_type);

        new = (struct function_type *) malloc(sizeof(struct function_type));
        if (new == NULL) {
            perror(malloc_failed);
            exit(1);
        }

        new->type = type;
        new->name = name;
        new->next = NULL;

        if (last == NULL)
            first = last = new;
        else {
            last->next = new;
            last = new;
        }

        if (!lex(state, &t))
            lex_error(state, expected_comma_or_paren);

        if (t.kind != T_COMMA)
            break;
    }

    (*last_derivation)->type.derived.type.function = first;
}

const char *expected_args = "expected expression or `)'";
const char *undeclared_identifier = "undeclared identifier";
const char *expected_expression = "expected expression";
const char *no_variable_name = "variables must be named";
const char *duplicate_variable = "duplicate variable name";
const char *bad_operands = "operands must be arithmetic types";
const char *bad_lvalue = "assignment left-hand side must be an lvalue";
const char *expected_statement = "expected statement";

static struct node *parse_assignment_expression(
    struct lex_state *state,
    struct scope *scope
);

static struct node *parse_expression(
    struct lex_state *state,
    struct scope *scope
);

static struct node *parse_function_arguments(
    struct lex_state *state,
    struct scope *scope,
    struct token *ident
) {
    struct token t;
    struct node *new = NULL;

    if (!lex(state, &t))
        lex_error(state, expected_args);

    /* TODO: proper argument parsing */

    if (t.kind != T_CLOSE_PAREN)
        lex_error(state, expected_paren);

    new = (struct node *) malloc(sizeof(struct node));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }
    new->kind = N_CALL;
    new->references = 0;
    new->prev = NULL;
    /* TODO: get return type from function declaration */
    new->type = NULL;
    new->visited = 0;
    new->is_arith_type = 1;
    new->has_side_effects = 1;
    new->is_lvalue = 0;
    new->data.tag = token_to_string(state, ident);

    return new;
}

/* identifier
 * constant
 * string-literal
 * (expression) */
static struct node *parse_primary_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct token t, t2;
    struct node *new = NULL;
    char *name;
    unsigned long hash_value;
    struct scope *current;
    struct variable *variable;

    if (!lex(state, &t))
        return NULL;

    switch (t.kind) {
        case T_IDENT:
            /* check whether function arguments follow and parse them.
             * technically this counts as a postfix expression, not a primary
             * expression, however primary expressions are only used in parsing
             * posfix expressions so it doesn't matter */
            if (lex(state, &t2))
                if (t2.kind == T_OPEN_PAREN)
                    return parse_function_arguments(state, scope, &t);
                else
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
                )
                    return variable->last_assignment;
            lex_error(state, undeclared_identifier);

            break;
        case T_NUMBER:
            new = (struct node *) malloc(sizeof(struct node));
            if (new == NULL) {
                perror(malloc_failed);
                exit(1);
            }
            new->kind = N_LITERAL;
            new->references = 0;
            new->prev = NULL;
            new->type = NULL;
            new->visited = 0;
            new->is_arith_type = 1;
            new->has_side_effects = 0;
            new->is_lvalue = 0;
            new->data.literal = token_to_number(state, &t, 10);
            break;
        case T_HEX_NUMBER:
            new = (struct node *) malloc(sizeof(struct node));
            if (new == NULL) {
                perror(malloc_failed);
                exit(1);
            }
            new->kind = N_LITERAL;
            new->references = 0;
            new->prev = NULL;
            new->type = NULL;
            new->visited = 0;
            new->is_arith_type = 1;
            new->has_side_effects = 0;
            new->is_lvalue = 0;
            new->data.literal = token_to_number(state, &t, 16);
            break;
        case T_OCT_NUMBER:
            new = (struct node *) malloc(sizeof(struct node));
            if (new == NULL) {
                perror(malloc_failed);
                exit(1);
            }
            new->kind = N_LITERAL;
            new->references = 0;
            new->prev = NULL;
            new->type = NULL;
            new->visited = 0;
            new->is_arith_type = 1;
            new->has_side_effects = 0;
            new->is_lvalue = 0;
            new->data.literal = token_to_number(state, &t, 8);
            break;
        case T_OPEN_PAREN:
            new = parse_expression(state, scope);
            if (new == NULL)
                lex_error(state, expected_expression);
            if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
                lex_error(state, expected_paren);
            break;
        default:
            lex_rewind(state);
            break;
    }

    return new;
}

/* primary-expression
 * postfix-expression [expression] 
 * postfix-expression (argument-expression-list<opt>) 
 * postfix-expression . identifier
 * postfix-expression -> identifier
 * postfix-expression ++ 
 * postfix-expression -- */
static struct node *parse_postfix_expression(
    struct lex_state *state,
    struct scope *scope
) {
    /* TODO: handle array/struct/union indexing, increment and decrement */
    return parse_primary_expression(state, scope);
}

/* postfix-expression
 * ++ unary-expression
 * -- unary-expression
 * unary-operator cast-expression
 * sizeof unary-expression
 * sizeof(type-name) */
static struct node *parse_unary_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct token t;

    if (!lex(state, &t))
        return NULL;

    switch (t.kind) {
        case T_INCREMENT:
            /* TODO: handle pre-increment */
        case T_DECREMENT:
            /* TODO: handle pre-decrement */
        case T_AMPERSAND:
            /* TODO: handle reference operator */
        case T_ASTERISK:
            /* TODO: handle dereference operator */
        case T_PLUS:
            /* TODO: handle plus operator */
        case T_MINUS:
            /* TODO: handle minus operator */
        case T_BITWISE_NOT:
            /* TODO: handle the bitwise NOT operator */
        case T_LOGICAL_NOT:
            /* TODO: handle the logical NOT operator */
        case T_SIZEOF:
            /* TODO: handle sizeof */
            lex_error(state, "unimplemented");
            break;
        default:
            lex_rewind(state);
            return parse_postfix_expression(state, scope);
    }
}

/* unary-expression
 * (type-name) cast-expression */
struct node *parse_cast_expression(
    struct lex_state *state,
    struct scope *scope
) {
    /* TODO: handle casting */
    return parse_unary_expression(state, scope);
}

static struct node *lhs_rhs_to_node(
    struct lex_state *state,
    struct node *lhs,
    struct node *rhs,
    enum node_kind kind
) {
    struct node *new;

    if (rhs == NULL)
        lex_error(state, expected_expression);

    if (!lhs->is_arith_type || !rhs->is_arith_type)
        lex_error(state, bad_operands);

    lhs->references ++;
    rhs->references ++;

    new = (struct node *) malloc(sizeof(struct node));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    new->kind = kind;
    new->references = 0;
    new->prev = NULL;

    /* figure out what type this node should have. if one side has an explicit
     * type and the other doesn't, then this node takes the explicit type. if
     * both sides have types, the type with the largest size is used */
    if (lhs->type != NULL && rhs->type == NULL)
        (new->type = lhs->type)->references ++;
    else if (lhs->type == NULL && rhs->type != NULL)
        (new->type = rhs->type)->references ++;
    else if (lhs->type != NULL && rhs->type != NULL)
        if (rhs->type->size > lhs->type->size)
            (new->type = rhs->type)->references ++;
        else
            (new->type = lhs->type)->references ++;
    else
        new->type = NULL;

    new->visited = 0;
    new->is_arith_type = 1;
    new->has_side_effects = lhs->has_side_effects || rhs->has_side_effects;
    new->is_lvalue = 0;
    new->deps.op.left = lhs;
    new->deps.op.right = rhs;

    return new;
}

/* cast-expression
 * multiplicative-expression * cast-expression
 * multiplicative-expression / cast-expression
 * multiplicative-expression % cast-expression */
static struct node *parse_mul_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_cast_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        switch (t.kind) {
            case T_ASTERISK:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_cast_expression(state, scope),
                    N_MUL
                );
                break;
            case T_DIVIDE:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_cast_expression(state, scope),
                    N_DIV
                );
                break;
            case T_MODULO:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_cast_expression(state, scope),
                    N_MOD
                );
                break;
            default:
                lex_rewind(state);
                return lhs;
        }
    }
}

/* multiplicative-expression
 * additive-expression + multiplicative-expression
 * additive-expression - multiplicative-expression */
static struct node *parse_add_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_mul_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        switch (t.kind) {
            case T_PLUS:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_mul_expression(state, scope),
                    N_ADD
                );
                break;
            case T_MINUS:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_mul_expression(state, scope),
                    N_SUB
                );
                break;
            default:
                lex_rewind(state);
                return lhs;
        }
    }
}

/* additive-expression
 * shift-expression << additive-expression
 * shift-expression >> additive-expression */
static struct node *parse_shift_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_add_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        switch (t.kind) {
            case T_LEFT_SHIFT:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_add_expression(state, scope),
                    N_SHIFT_LEFT
                );
                break;
            case T_RIGHT_SHIFT:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_add_expression(state, scope),
                    N_SHIFT_RIGHT
                );
                break;
            default:
                lex_rewind(state);
                return lhs;
        }
    }
}

/* shift-expression
 * relational-expression < shift-expression
 * relational-expression > shift-expression
 * relational-expression <= shift-expression
 * relational-expression >= shift-expression */
static struct node *parse_rel_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_shift_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        switch (t.kind) {
            case T_LESS_THAN:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_shift_expression(state, scope),
                    N_LESS
                );
                break;
            case T_GREATER_THAN:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_shift_expression(state, scope),
                    N_GREATER
                );
                break;
            case T_LESS_EQ:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_shift_expression(state, scope),
                    N_LESS_EQ
                );
                break;
            case T_GREATER_EQ:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_shift_expression(state, scope),
                    N_GREATER_EQ
                );
                break;
            default:
                lex_rewind(state);
                return lhs;
        }
    }
}

/* relational-expression
 * equality-expression == relational-expression
 * equality-expression != relational-expression */
static struct node *parse_eq_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_rel_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        switch (t.kind) {
            case T_EQUALS:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_rel_expression(state, scope),
                    N_EQUALS
                );
                break;
            case T_NOT_EQ:
                lhs = lhs_rhs_to_node(
                    state,
                    lhs,
                    parse_rel_expression(state, scope),
                    N_NOT_EQ
                );
                break;
            default:
                lex_rewind(state);
                return lhs;
        }
    }
}

/* equality-expression
 * AND-expression & equality-expression */
static struct node *parse_bitwise_and_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_eq_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        if (t.kind == T_AMPERSAND)
            lhs = lhs_rhs_to_node(
                state,
                lhs,
                parse_eq_expression(state, scope),
                N_BITWISE_AND
            );
        else {
            lex_rewind(state);
            return lhs;
        }
    }
}

/* AND-expression
 * exclusive-OR-expression ^ AND-expression */
static struct node *parse_bitwise_xor_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_bitwise_and_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        if (t.kind == T_BITWISE_XOR)
            lhs = lhs_rhs_to_node(
                state,
                lhs,
                parse_bitwise_and_expression(state, scope),
                N_BITWISE_XOR
            );
        else {
            lex_rewind(state);
            return lhs;
        }
    }
}

/* exclusive-OR-expression
 * inclusive-OR-expression | exclusive-OR-expression */
static struct node *parse_bitwise_or_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_bitwise_xor_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        if (t.kind == T_BITWISE_OR)
            lhs = lhs_rhs_to_node(
                state,
                lhs,
                parse_bitwise_xor_expression(state, scope),
                N_BITWISE_OR
            );
        else {
            lex_rewind(state);
            return lhs;
        }
    }
}

/* inclusive-OR-expression
 * logical-AND-expression && inclusive-OR-expression */
static struct node *parse_logical_and_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_bitwise_or_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        if (t.kind == T_LOGICAL_AND)
            lhs = lhs_rhs_to_node(
                state,
                lhs,
                parse_bitwise_or_expression(state, scope),
                N_AND
            );
        else {
            lex_rewind(state);
            return lhs;
        }
    }
}

/* logical-AND-expression
 * logical-OR-expression || logical-AND-expression */
static struct node *parse_logical_or_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_logical_and_expression(state, scope);
    struct token t;

    while (1) {
        if (!lex(state, &t))
            return lhs;

        if (t.kind == T_LOGICAL_OR)
            lhs = lhs_rhs_to_node(
                state,
                lhs,
                parse_logical_and_expression(state, scope),
                N_OR
            );
        else {
            lex_rewind(state);
            return lhs;
        }
    }
}

/* logical-OR-expression
 * logical-OR-expression ? expression : conditional-expression */
static struct node *parse_conditional_expression(
    struct lex_state *state,
    struct scope *scope
) {
    /* TODO */
    return parse_logical_or_expression(state, scope);
}

static struct node *parse_variable_assignment(
    struct lex_state *state,
    struct scope *scope,
    struct variable *variable
) {
    struct node *new = (struct node *) malloc(sizeof(struct node));
    if (new == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    new->kind = N_LVALUE;
    if (
        (new->deps.single = parse_assignment_expression(state, scope))
        == NULL
    )
        lex_error(state, expected_expression);
    new->deps.single->references ++;
    new->prev = NULL;
    new->data.lvalue = variable;
    variable->references ++;
    new->type = variable->type;
    new->references = 1; /* single reference is last_assignment */
    new->visited = 0;
    new->is_arith_type =
        variable->type->top == TOP_BASIC &&
        variable->type->type.basic.type_specifier <= TY_LONG_LONG;
    /* ordering will be forced for this node, so nodes don't need to
     * inherit side effects from it */
    new->has_side_effects = 0;
    new->is_lvalue = 1;

    if (variable->last_assignment != NULL) {
        variable->last_assignment->references --;
        free_node(variable->last_assignment);
    }

    variable->last_assignment = new;

    /* add ordering edge for nodes with side effects */
    if (new->deps.single->has_side_effects) {
        new->prev = scope->last_side_effect;

        scope->last_side_effect = new;
        new->references ++;
    }

    return new;
}

/* conditional-expression
 * unary-expression assignment-operator assignment-expression
 * (this parser doesn't allow parsing strictly a unary expression, so anything
 * that produces an lvalue is accepted) */
static struct node *parse_assignment_expression(
    struct lex_state *state,
    struct scope *scope
) {
    struct node *lhs = parse_conditional_expression(state, scope);
    struct token t;

    if (!lex(state, &t))
        return lhs;

    if (t.kind == T_ASSIGN) {
        if (!lhs->is_lvalue)
            lex_error(state, bad_lvalue);

        if (lhs->data.lvalue != NULL)
            return parse_variable_assignment(state, scope, lhs->data.lvalue);

        lex_error(state, "unimplemented");
    } else {
        lex_rewind(state);
        return lhs;
    }
}

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
            if (new->has_side_effects) {
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
        variable->first_load = NULL;
        variable->last_assignment = NULL;
        variable->phi_list = NULL;
        variable->parent = NULL;
        variable->references = 1;

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
            new->visited = 0;
            new->is_arith_type =
                variable->type->top == TOP_BASIC &&
                variable->type->type.basic.type_specifier <= TY_LONG_LONG;
            new->has_side_effects = 0;
            new->is_lvalue = 1;

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

                if (new->has_side_effects) {
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
            lex_error(state, "unimplemented");
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
            new->visited = 0;
            new->is_arith_type = 0;
            new->has_side_effects = 1;
            new->is_lvalue = 0;

            if (!lex(state, &t) || t.kind != T_SEMICOLON)
                lex_error(state, expected_semicolon);

            return new;
    }

    /* expression-statement */
    return parse_expression_statement(state, scope);
}

int parse(struct lex_state *state) {
    debug_graph(parse_statement(state, NULL));
}
