#include <stdio.h>
#include <stdlib.h>
#include "ir.h"
#include "parser.h"
#include "lexer.h"

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
const char *unknown_size = "storage size of field is unknown";
const char *field_missing_name = "field missing name";
const char *expected_paren = "expected `)'";
const char *expected_comma_or_paren = "expected `,' or `)'";

/* given a token, read the area in the file it points to into a newly allocated
 * null terminated string and return it */
static char *token_to_string(struct lex_state *state, struct token *t) {
    char *string;
    unsigned int len, len2;

    len = t->file_end - t->file_start;
    string = (char *) malloc(len + 1);
    if (string == NULL) {
        perror(malloc_failed);
        exit(1);
    }

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
static unsigned long parse_number(
    struct lex_state *state,
    struct token *t,
    unsigned long base
) {
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

static struct struct_union_field *parse_struct_union(struct lex_state *state);

/* macros to deduplicate wacky code in parse_standard_type() */

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

/* parses a "standard type" signature (i.e. anything in enum type_specifier)
 * and its storage class, sign specifier, and qualifiers. returns 1 on success,
 * 0 on failure. contents of the type pointer will only be modified on success
 */
static char parse_standard_type(struct lex_state *state, struct type *type) {
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

    type->top = TOP_NORMAL;
    type->type.standard.type_specifier = type_specifier;
    type->type.standard.storage_class = storage_class;
    type->type.standard.sign_specifier = sign_specifier;
    type->type.standard.const_qualified = const_qualified;
    type->type.standard.volatile_qualified =
        volatile_qualified;
    if (type->type.standard.has_fields = have_fields)
        type->type.standard.name_field_ptr = first_field;
    else
        type->type.standard.name_field_ptr = name;
    type->size = size;

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

    if (*first_derivation == NULL)
        *first_derivation = *last_derivation = new;
    else {
        (*first_derivation)->type.derived.derivation = new;
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
        parse_number(state, &t, base);
    if (!lex(state, &t) || t.kind != T_CLOSE_BRACKET)
        lex_error(state, bad_array_syntax);
}

static void parse_array(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
) {
    struct token t;
    struct pointer_type *p;
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

static void parse_function_arguments(
    struct lex_state *state,
    struct type **first_derivation,
    struct type **last_derivation
);

static char parse_type_after_name(
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
            parse_function_arguments(state, first_derivation, last_derivation);
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
                parse_type_derivation(
                    state,
                    &recursive_first,
                    &recursive_last,
                    name
                );

                if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
                    lex_error(state, expected_paren);

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
                return 1;
        }

    }
}


/* parse a type signature. this code is very wacky, just like c type signatures
 * TODO: explain type signature syntax
 */
static char parse_type_signature(
    struct lex_state *state,
    struct type **type,
    char **name
) {
    struct token t;
    struct type *first_derivation = NULL;
    struct type *last_derivation = NULL;
    struct type *current;

    if ((*type = (struct type *) malloc(sizeof(struct type))) == NULL) {
        perror(malloc_failed);
        exit(1);
    }

    *name = NULL;

    if (!parse_standard_type(state, *type))
        return 0;

    if (!parse_type_derivation(
        state,
        &first_derivation,
        &last_derivation,
        name
    ))
        return 1;

    /* merge the standard type into the derivation list */
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
                    current->top == TOP_NORMAL ||
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

static void parse_function_arguments(
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

    while (1) {
        struct function_type *new;

        if (!parse_type_signature(state, &type, &name)) {
            if (!lex(state, &t) || t.kind != T_CLOSE_PAREN)
                lex_error(state, expected_paren);

            return;
        }

        if ((new = malloc(sizeof(struct function_type))) == NULL) {
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

        switch (t.kind) {
            case T_COMMA:
                continue;
            case T_CLOSE_PAREN:
                break;
        }

        break;
    }

    (*last_derivation)->type.derived.type.function = first;
}

int parse(struct lex_state *state) {
    struct type *uwu;
    char *name;
    struct token t;
    if (!parse_type_signature(state, &uwu, &name))
        lex_error(state, "expected type signature");
    print_type(uwu, name);
    printf("size is %d, name is %s\n", uwu->size, name);
    if (!lex(state, &t))
        lex_error(state, "unexpected eof");
    printf("next token is %d\n", t.kind);
    if (lex(state, &t))
        printf("next token is %d\n", t.kind);
    else
        printf("nothing after\n");
    return 1;
}
