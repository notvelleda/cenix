#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "lexer.h"

/* given a token, read the area in the file it points to into a newly allocated
 * null terminated string and return it */
static char *token_to_string(struct lex_state *state, struct token *t) {
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

/* given a token, read the area in the file it points to and parse it into a
 * number of the given base, up to base 16 */
/* TODO: add binary notation (0b...) support since it'll be easy */
static unsigned long parse_number(struct lex_state *state, struct token *t,
                                  unsigned long base) {
    unsigned long result = 0;
    unsigned int len, len2, i = 0;
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

/* duplicates a type, safely allocating memory for it in the process */
static struct type *dup_type(struct type *type) {
    struct type *new;
    new = (struct type *) malloc(sizeof(struct type));
    if (new == NULL) {
        perror("failed to allocate memory");
        exit(1);
    }
    *new = *type;
    return new;
}

/* error messages that can be generated by read_type_signature() */
const char *dup_type_spec_err = "duplicate type specifier";
const char *dup_storage_err = "duplicate storage class";
const char *dup_sign_err = "duplicate sign specifier";
const char *dup_type_qual_err = "duplicate type qualifier";
const char *bad_array_syntax = "expected number or `]'";
const char *no_type_spec = "expected type specifier";

/* macros to deduplicate wacky code in read_type_signature() */

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
/* same as SET_STORAGE_CLASS() but for type qualifier */
#define SET_TYPE_QUALIFIER(q) {\
    if (type_qualifier != Q_NONE)\
        lex_error(state, dup_type_qual_err);\
    type_qualifier = q;\
}
/* set the have_set_type flag and populate the output type with the values that
 * have been found during parsing */
#define SET_TYPE() {\
    have_set_type = 1;\
    type->top_type = TOP_NORMAL;\
    type->type.standard_type.type_specifier = type_specifier;\
    type->type.standard_type.storage_class = storage_class;\
    type->type.standard_type.sign_specifier = sign_specifier;\
    type->type.standard_type.type_qualifier = type_qualifier;\
}
/* allocate memory for a new type object, copy the current type in there, then
 * set up the derivation field on the new type object to point to it. also sets
 * the new top type accordingly */
#define DERIVE_TYPE(t, f) {\
    other_type = dup_type(type);\
    type->top_type = t;\
    type->type.f.derivation = other_type;\
}
/* derive an array type, parse its length, and ensure a closing bracket follows
 */
#define PARSE_ARRAY(b) {\
    DERIVE_TYPE(TOP_ARRAY, array_type);\
    type->type.array_type.length = parse_number(state, &t, b);\
    if (!lex(state, &t) || t.kind != T_CLOSE_BRACKET)\
        lex_error(state, bad_array_syntax);\
}

/* parse a type signature. this code is very wacky, just like c type signatures
 * TODO: explain type signature syntax
 */
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
            /* might not be the best idea to completely give up here on eof,
             * but it's not like any type will be produced either
             * TODO: maybe handle this like the default switch case? */
            return NULL;

        switch (t.kind) {
            case T_VOID:
                SET_TYPE_SPECIFIER(TY_VOID);
                break;
            case T_CHAR:
                SET_TYPE_SPECIFIER(TY_CHAR);
                break;
            case T_SHORT:
                SET_TYPE_SPECIFIER(TY_SHORT);
                break;
            case T_LONG:
                if (have_type_specifier)
                    if (type_specifier == TY_LONG)
                        type_specifier = TY_LONG_LONG;
                    else
                        lex_error(state, dup_type_spec_err);

                have_type_specifier = 1;
                type_specifier = TY_LONG;
                break;
            case T_INT:
                if (have_type_specifier && type_specifier != TY_SHORT &&
                    type_specifier != TY_LONG && type_specifier != TY_LONG_LONG)
                    lex_error(state, dup_type_spec_err);

                have_type_specifier = 1;
                type_specifier = TY_INT;
                break;
            case T_EXTERN:
                SET_STORAGE_CLASS(C_EXTERN);
                break;
            case T_STATIC:
                SET_STORAGE_CLASS(C_STATIC);
                break;
            case T_UNSIGNED:
                SET_SIGN_SPECIFIER(S_UNSIGNED);
                break;
            case T_SIGNED:
                SET_SIGN_SPECIFIER(S_SIGNED);
                break;
            case T_CONST:
                SET_TYPE_QUALIFIER(Q_CONST);
                break;
            case T_VOLATILE:
                SET_TYPE_QUALIFIER(Q_VOLATILE);
                break;
            case T_ASTERISK:
                if (!have_set_type)
                    SET_TYPE();

                DERIVE_TYPE(TOP_POINTER, pointer_type);
                type_qualifier = Q_NONE;
                break;
            case T_IDENT:
                if (!have_type_specifier && storage_class == C_AUTO &&
                    type_qualifier == Q_NONE) {
                    lex_rewind(state);
                    return NULL;
                }

                if (!have_set_type)
                    SET_TYPE() /* wacky macro syntax */
                else if (type->top_type == TOP_POINTER)
                    /* handle qualifier for the pointer variable itself (not the
                     * type it's pointing to)
                     * this is why const char * and char *const are different */
                    type->type.pointer_type.qualifier = type_qualifier;

                type_name = token_to_string(state, &t);

                /* try to parse either an array or [] pointer notation */
                if (!lex(state, &t))
                    return type_name;

                if (t.kind != T_OPEN_BRACKET) {
                    lex_rewind(state);
                    return type_name;
                }

                if (!lex(state, &t))
                    lex_error(state, bad_array_syntax);

                switch (t.kind) {
                    case T_NUMBER:
                        PARSE_ARRAY(10);
                        break;
                    case T_HEX_NUMBER:
                        PARSE_ARRAY(16);
                        break;
                    case T_OCT_NUMBER:
                        PARSE_ARRAY(8);
                        break;
                    case T_CLOSE_BRACKET:
                        /* [] is parsed just like a pointer */
                        DERIVE_TYPE(TOP_POINTER, pointer_type);
                        type->type.pointer_type.qualifier = Q_NONE;
                        break;
                    default:
                        lex_error(state, bad_array_syntax);
                }

                return type_name;
            case T_OPEN_BRACKET:
                /* TODO: set type and parse array syntax without setting type
                 * name
                 * code here can probably be similar to array parsing above,
                 * just need to figure out how unnnamed types are handled */
                lex_error(state, "unimplemented");
            default:
                /* rewind the lexer to pass this token back to whatever called
                 * this function */
                lex_rewind(state);

                if (!have_type_specifier && storage_class == C_AUTO &&
                    sign_specifier == S_UNKNOWN && type_qualifier == Q_NONE)
                    /* probably nothing has been parsed, so give up */
                    return NULL;

                /* otherwise something has likely been parsed, make sure it's
                 * enough to populate a proper type */
                if (!have_set_type) {
                    if (!have_type_specifier)
                        lex_error(state, no_type_spec);

                    SET_TYPE();
                }

                /* TODO: figure out how to handle nameless types */
                return NULL;
        }
    }
}

#if 0
static enum type read_type_signature(struct lex_state *state) {
    struct token t;

    if (!lex(state, &t))
        lex_error(state, "expected type signature");

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
        case T_SEMICOLON:
            return 1;
        case T_RETURN:
            if (!lex(state, &t) || t.kind != T_NUMBER)
                lex_error(state, "expected number");
            break;
        default:
            lex_error(state, "expected identifier or semicolon");
    }

    if (!lex(state, &t) || t.kind != T_SEMICOLON)
        lex_error(state, "expected semicolon");
}

/* parses a block of code enclosed by curly brackets */
static int parse_block(struct lex_state *state) {
    struct token t;

    if (!lex(state, &t) || t.kind != T_OPEN_CURLY)
        lex_error(state, "expected `{'");

    while (1) {
        if (!lex(state, &t))
            lex_error(state, "unexpected eof");

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
    if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
        lex_error(state, "arguments aren't supported yet");

    return parse_block(state);
}
#endif

int parse(struct lex_state *state) {
    struct type uwu;
    struct token t;
    if (!read_type_signature(state, &uwu))
        lex_error(state, "expected type signature");
    if (!lex(state, &t))
        lex_error(state, "unexpected eof");
    printf("next token is %d\n", t.kind);
    if (lex(state, &t))
        printf("next token is %d\n", t.kind);
    else
        printf("nothing after\n");
    return 1;

    /*enum type sig;
    struct token name, t;

    if (!(sig = read_type_signature(state)))
        lex_error(state, "expected type signature");

    if (!lex(state, &t) || t.kind != T_IDENT)
        lex_error(state, "expected identifier, not keyword");

    if (!lex(state, &t))
        lex_error(state, "unexpected eof");

    switch (t.kind) {
        case T_OPEN_PAREN:
            return parse_function(state);
            break;
        default:
            lex_error(state, "unexpected token");
    }*/
}
