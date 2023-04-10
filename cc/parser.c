#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "lexer.h"

static char *ident_to_string(struct lex_state *state, struct token *t) {
    long i;
    char *string;
    unsigned int len, len2;

    len = t->file_end - t->file_start;
    string = (char *) malloc(len + 1);

    fseek(state->stream, t->file_start, SEEK_SET);
    if ((len2 = fread(string, 1, len, state->stream)) != len) {
        free(string);
        return NULL;
    }
    string[len] = 0;

    return string;
}

const char *dup_type_spec_err = "duplicate type specifier";
const char *dup_storage_err = "duplicate storage class";
const char *dup_sign_err = "duplicate sign specifier";
const char *dup_type_qual_err = "duplicate type qualifier";
const char *bad_array_syntax = "expected number or `]'";

static char *read_type_signature(struct lex_state *state, struct type *type) {
    struct token t;
    char *name = NULL;
    char *type_name = NULL;
    enum type_specifier type_specifier;
    char have_type_specifier = 0;
    enum storage_class storage_class = C_AUTO;
    enum sign_specifier sign_specifier = S_UNKNOWN;
    enum type_qualifier type_qualifier = Q_NONE;
    char have_set_type = 0;
    struct type *other_type;

    while (1) {
        if (!lex(state, &t))
            return NULL;

        switch (t.kind) {
            case T_VOID:
                if (have_type_specifier) {
                    lex_error(state, dup_type_spec_err);
                    exit(1);
                }

                have_type_specifier = 1;
                type_specifier = TY_VOID;
                break;
            case T_CHAR:
                if (have_type_specifier) {
                    lex_error(state, dup_type_spec_err);
                    exit(1);
                }

                have_type_specifier = 1;
                type_specifier = TY_CHAR;
                break;
            case T_SHORT:
                if (have_type_specifier) {
                    lex_error(state, dup_type_spec_err);
                    exit(1);
                }

                have_type_specifier = 1;
                type_specifier = TY_SHORT;
                break;
            case T_LONG:
                if (have_type_specifier) {
                    if (type_specifier == TY_LONG)
                        type_specifier = TY_LONG_LONG;
                    else {
                        lex_error(state, dup_type_spec_err);
                        exit(1);
                    }
                }

                have_type_specifier = 1;
                type_specifier = TY_LONG;
                break;
            case T_INT:
                if (have_type_specifier && type_specifier != TY_SHORT &&
                    type_specifier != TY_LONG && type_specifier != TY_LONG_LONG)
                {
                    lex_error(state, dup_type_spec_err);
                    exit(1);
                }

                have_type_specifier = 1;
                type_specifier = TY_INT;
                break;
            case T_EXTERN:
                if (storage_class != C_AUTO) {
                    lex_error(state, dup_storage_err);
                    exit(1);
                }

                storage_class = C_EXTERN;
                break;
            case T_STATIC:
                if (storage_class != C_AUTO) {
                    lex_error(state, dup_storage_err);
                    exit(1);
                }

                storage_class = C_STATIC;
                break;
            case T_UNSIGNED:
                if (sign_specifier != S_UNKNOWN) {
                    lex_error(state, dup_sign_err);
                    exit(1);
                }

                sign_specifier = S_UNSIGNED;
                break;
            case T_SIGNED:
                if (sign_specifier != S_UNKNOWN) {
                    lex_error(state, dup_sign_err);
                    exit(1);
                }

                sign_specifier = S_SIGNED;
                break;
            case T_CONST:
                if (type_qualifier != Q_NONE) {
                    lex_error(state, dup_type_qual_err);
                    exit(1);
                }

                type_qualifier = Q_CONST;
                break;
            case T_VOLATILE:
                if (type_qualifier != Q_NONE) {
                    lex_error(state, dup_type_qual_err);
                    exit(1);
                }

                type_qualifier = Q_VOLATILE;
                break;
            case T_ASTERISK:
                if (!have_set_type) {
                    have_set_type = 1;
                    type->top_type = TOP_NORMAL;
                    type->type.standard_type.type_specifier = type_specifier;
                    type->type.standard_type.storage_class = storage_class;
                    type->type.standard_type.sign_specifier = sign_specifier;
                    type->type.standard_type.type_qualifier = type_qualifier;
                }

                other_type = (struct type *) malloc(sizeof(struct type));
                *other_type = *type;
                type->top_type = TOP_POINTER;
                type->type.pointer_type.derivation = other_type;
                type_qualifier = Q_NONE;
                break;
            case T_IDENT:
                if (!have_type_specifier)
                    return NULL;

                if (!have_set_type) {
                    have_set_type = 1;
                    type->top_type = TOP_NORMAL;
                    type->type.standard_type.type_specifier = type_specifier;
                    type->type.standard_type.storage_class = storage_class;
                    type->type.standard_type.sign_specifier = sign_specifier;
                    type->type.standard_type.type_qualifier = type_qualifier;
                } else {
                    switch (type->top_type) {
                        case TOP_POINTER:
                            type->type.pointer_type.qualifier = type_qualifier;
                            break;
                    }
                }

                type_name = ident_to_string(state, &t);

                /* try to parse either an array or [] pointer notation */
                if (!lex(state, &t))
                    return type_name;

                if (t.kind != T_OPEN_BRACKET) {
                    lex_rewind(state);
                    return type_name;
                }

                if (!lex(state, &t)) {
                    lex_error(state, bad_array_syntax);
                    exit(1);
                }

                switch (t.kind) {
                    case T_NUMBER:
                    case T_HEX_NUMBER:
                    case T_OCT_NUMBER:
                        /* TODO: parse length to unsigned int and set top type */
                        break;
                    case T_CLOSE_BRACKET:
                        /* [] is parsed just like a pointer */
                        other_type = (struct type *)
                            malloc(sizeof(struct type));
                        *other_type = *type;
                        type->top_type = TOP_POINTER;
                        type->type.pointer_type.derivation = other_type;
                        type->type.pointer_type.qualifier = Q_NONE;
                        break;
                    default:
                        lex_error(state, bad_array_syntax);
                        exit(1);
                }

                return type_name;
            default:
                return NULL;
        }
    }
}

#if 0
static enum type read_type_signature(struct lex_state *state) {
    struct token t;

    if (!lex(state, &t)) {
        lex_error(state, "expected type signature");
        exit(1);
    }

    switch (t.kind) {
        case T_INT:
            return TY_INT;
        default:
            return TY_UNKNOWN;
    }
}

/* parses a semicolon-terminated statement */
static int parse_statement(struct lex_state *state, struct token *first) {
    struct token t;

    switch (first->kind) {
        case T_IDENT:
            lex_error(state, "TODO: ident support");
            exit(1);
        case T_SEMICOLON:
            return 1;
        case T_RETURN:
            if (!lex(state, &t) || t.kind != T_NUMBER) {
                lex_error(state, "expected number");
                exit(1);
            }
            break;
        default:
            lex_error(state, "expected identifier or semicolon");
            exit(1);
    }

    if (!lex(state, &t) || t.kind != T_SEMICOLON) {
        lex_error(state, "expected semicolon");
        exit(1);
    }
}

/* parses a block of code enclosed by curly brackets */
static int parse_block(struct lex_state *state) {
    struct token t;

    if (!lex(state, &t) || t.kind != T_OPEN_CURLY) {
        lex_error(state, "expected `{'");
        exit(1);
    }

    while (1) {
        if (!lex(state, &t)) {
            lex_error(state, "unexpected eof");
            exit(1);
        }

        switch (t.kind) {
            case T_CLOSE_CURLY:
                return 1;
            default:
                parse_statement(state, &t);
                break;
        }
    }
}

/* parses the arguments and body of a function */
static int parse_function(struct lex_state *state) {
    struct token t;

    /* arguments aren't supported yet */
    if (!lex(state, &t) || t.kind != T_CLOSE_PAREN) {
        lex_error(state, "arguments aren't supported yet");
        exit(1);
    }

    return parse_block(state);
}
#endif

int parse(struct lex_state *state) {
    struct type uwu;
    if (!read_type_signature(state, &uwu)) {
        lex_error(state, "expected type signature");
        exit(1);
    }
    return 1;

    /*enum type sig;
    struct token name, t;

    if (!(sig = read_type_signature(state))) {
        lex_error(state, "expected type signature");
        exit(1);
    }

    if (!lex(state, &t) || t.kind != T_IDENT) {
        lex_error(state, "expected identifier, not keyword");
        exit(1);
    }

    if (!lex(state, &t)) {
        lex_error(state, "unexpected eof");
        exit(1);
    }

    switch (t.kind) {
        case T_OPEN_PAREN:
            return parse_function(state);
            break;
        default:
            lex_error(state, "unexpected token");
            exit(1);
    }*/
}
