#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

void lex_error(struct lex_state* state, const char* message) {
    fprintf(stderr, "%s:%d: error: %s\n", state->filename, state->line,
        message);
    exit(1);
}

static char parse_single_char(struct lex_state *state, char ending) {
    char c;

    if (!fread(&c, 1, 1, state->stream))
        return 0;

    if (c == ending)
        return 0;
    else if (c == '\\')
        /* skip the next char as it's escaped */
        fseek(state->stream, 1, SEEK_CUR);
    else if (c == '\n')
        lex_error(state, "unexpected newline");

    return 1;
}

static char next_char(FILE *stream) {
    char c;

    if (!fread(&c, 1, 1, stream)) {
        /* avoids repeating the last character on eof */
        fseek(stream, 1, SEEK_CUR);
        return 0;
    }

    if (isalpha(c) || c == '_' || isdigit(c))
        return c;
    else
        return 0;
}

static int cmp_string(FILE *stream, const char *s) {
    char c;

    while ((c = next_char(stream)) == *(s ++))
        if (!c) {
            fseek(stream, -1, SEEK_CUR);
            return 0;
        }

    fseek(stream, -1, SEEK_CUR);
    return c - *(-- s);
}

static void parse_number(FILE *stream, struct token *out) {
    char c;

    out->kind = T_NUMBER;
    while (fread(&c, 1, 1, stream) && isdigit(c));
    fseek(stream, -1, SEEK_CUR);
}

static void parse_ident(FILE *stream, struct token *out) {
    char c;

    out->kind = T_IDENT;
    while (fread(&c, 1, 1, stream) && (isalnum(c) || c == '_'));
    fseek(stream, -1, SEEK_CUR);
}

char lex(struct lex_state *state, struct token *out) {
    char c;

    if (state->repeat) {
        state->repeat = 0;
        out->kind = state->current.kind;
        out->file_start = state->current.file_start;
        out->file_end = state->current.file_end;
        out->line = state->current.line;
        return 1;
    }

    while (1) {
        out->file_start = ftell(state->stream);
        state->current.line = out->line = state->line;

        /* try reading 1 byte from the file and return if unsuccessful */
        if (!fread(&c, 1, 1, state->stream))
            return 0;

        switch (c) {
            case '(':
                out->kind = T_OPEN_PAREN;
                break;
            case ')':
                out->kind = T_CLOSE_PAREN;
                break;
            case '[':
                out->kind = T_OPEN_BRACKET;
                break;
            case ']':
                out->kind = T_CLOSE_BRACKET;
                break;
            case '{':
                out->kind = T_OPEN_CURLY;
                break;
            case '}':
                out->kind = T_CLOSE_CURLY;
                break;
            case ';':
                out->kind = T_SEMICOLON;
                break;
            case ':':
                out->kind = T_COLON;
                break;
            case ',':
                out->kind = T_COMMA;
                break;
            case '*':
                out->kind = T_ASTERISK;
                break;
            case '.':
                out->kind = T_PERIOD;
                break;
            case '-':
                if (fread(&c, 1, 1, state->stream)) {
                    switch (c) {
                        case '>':
                            out->kind = T_ARROW;
                            break;
                        case '-':
                            out->kind = T_DECREMENT;
                            break;
                        default:
                            out->kind = T_MINUS;
                            fseek(state->stream, -1, SEEK_CUR);
                    }
                } else {
                    out->kind = T_MINUS;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '+':
                if (fread(&c, 1, 1, state->stream) && c == '+') {
                    out->kind = T_INCREMENT;
                } else {
                    out->kind = T_ADD;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '=':
                if (fread(&c, 1, 1, state->stream) && c == '=') {
                    out->kind = T_EQUALS;
                } else {
                    out->kind = T_ASSIGN;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '/':
                out->kind = T_DIVIDE;
                break;
            case '%':
                out->kind = T_MODULO;
                break;
            case '!':
                if (fread(&c, 1, 1, state->stream) && c == '=') {
                    out->kind = T_NOT_EQ;
                } else {
                    out->kind = T_LOGICAL_NOT;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '<':
                if (fread(&c, 1, 1, state->stream)) {
                    switch (c) {
                        case '=':
                            out->kind = T_LESS_EQ;
                            break;
                        case '<':
                            out->kind = T_LEFT_SHIFT;
                            break;
                        default:
                            out->kind = T_LESS_THAN;
                            fseek(state->stream, -1, SEEK_CUR);
                    }
                } else {
                    out->kind = T_LESS_THAN;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '>':
                if (fread(&c, 1, 1, state->stream)) {
                    switch (c) {
                        case '=':
                            out->kind = T_GREATER_EQ;
                            break;
                        case '>':
                            out->kind = T_RIGHT_SHIFT;
                            break;
                        default:
                            out->kind = T_GREATER_THAN;
                            fseek(state->stream, -1, SEEK_CUR);
                    }
                } else {
                    out->kind = T_GREATER_THAN;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '&':
                if (fread(&c, 1, 1, state->stream) && c == '&') {
                    out->kind = T_LOGICAL_AND;
                } else {
                    out->kind = T_AMPERSAND;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '|':
                if (fread(&c, 1, 1, state->stream) && c == '|') {
                    out->kind = T_LOGICAL_OR;
                } else {
                    out->kind = T_BITWISE_OR;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case '^':
                out->kind = T_BITWISE_XOR;
                break;
            case '~':
                out->kind = T_BITWISE_NOT;
                break;
            case '?':
                out->kind = T_TERNARY;
                break;
            case '0':
                if (!fread(&c, 1, 1, state->stream)) {
                    out->kind = T_NUMBER;
                    fseek(state->stream, -1, SEEK_CUR);
                } else if (c == 'x' || c == 'X') {
                    /* parse hexadecimal digits */
                    out->kind = T_HEX_NUMBER;
                    out->file_start = ftell(state->stream);
                    if (!fread(&c, 1, 1, state->stream) || !isxdigit(c))
                        lex_error(state, "expected hex digits");
                    while (fread(&c, 1, 1, state->stream) && isxdigit(c));
                    fseek(state->stream, -1, SEEK_CUR);
                } else if (c >= '0' && c <= '7') {
                    /* parse octal digits */
                    out->kind = T_OCT_NUMBER;
                    out->file_start = ftell(state->stream) - 1;
                    if (!fread(&c, 1, 1, state->stream) || c < '0'
                        || c > '7')
                        lex_error(state, "expected octal digits");
                    while (fread(&c, 1, 1, state->stream) && c >= '0' &&
                        c <= '7');
                    fseek(state->stream, -1, SEEK_CUR);
                } else if (isdigit(c)) {
                    parse_number(state->stream, out);
                } else {
                    out->kind = T_NUMBER;
                    fseek(state->stream, -1, SEEK_CUR);
                }
                break;
            case 'b':
                if (!cmp_string(state->stream, "reak"))
                    out->kind = T_BREAK;
                else
                    parse_ident(state->stream, out);
                break;
            case 'c':
                switch (next_char(state->stream)) {
                    case 'a':
                        if (!cmp_string(state->stream, "se"))
                            out->kind = T_CASE;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'h':
                        if (!cmp_string(state->stream, "ar"))
                            out->kind = T_CHAR;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'o':
                        if (next_char(state->stream) == 'n') {
                            switch (next_char(state->stream)) {
                                case 's':
                                    if (!cmp_string(state->stream, "t"))
                                        out->kind = T_CONST;
                                    else
                                        parse_ident(state->stream, out);
                                    break;
                                case 't':
                                    if (!cmp_string(state->stream, "inue"))
                                        out->kind = T_CONTINUE;
                                    else
                                        parse_ident(state->stream, out);
                                    break;
                                default:
                                    fseek(state->stream, -1, SEEK_CUR);
                                    parse_ident(state->stream, out);
                            }
                        } else {
                            fseek(state->stream, -1, SEEK_CUR);
                            parse_ident(state->stream, out);
                        }
                        break;
                    default:
                        fseek(state->stream, -1, SEEK_CUR);
                        parse_ident(state->stream, out);
                }
                break;
            case 'd':
                switch (next_char(state->stream)) {
                    case 'e':
                        if (!cmp_string(state->stream, "fault"))
                            out->kind = T_DEFAULT;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'o':
                        if (!next_char(state->stream)) {
                            fseek(state->stream, -1, SEEK_CUR);
                            out->kind = T_DO;
                        } else {
                            fseek(state->stream, -1, SEEK_CUR);
                            parse_ident(state->stream, out);
                        }
                        break;
                    default:
                        fseek(state->stream, -1, SEEK_CUR);
                        parse_ident(state->stream, out);
                }
                break;
            case 'e':
                switch (next_char(state->stream)) {
                    case 'l':
                        if (!cmp_string(state->stream, "se"))
                            out->kind = T_ELSE;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'n':
                        if (!cmp_string(state->stream, "um"))
                            out->kind = T_ENUM;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'x':
                        if (!cmp_string(state->stream, "tern"))
                            out->kind = T_EXTERN;
                        else
                            parse_ident(state->stream, out);
                        break;
                    default:
                        fseek(state->stream, -1, SEEK_CUR);
                        parse_ident(state->stream, out);
                }
                break;
            case 'f':
                if (!cmp_string(state->stream, "or"))
                    out->kind = T_FOR;
                else
                    parse_ident(state->stream, out);
                break;
            case 'i':
                switch (next_char(state->stream)) {
                    case 'f':
                        if (!next_char(state->stream)) {
                            fseek(state->stream, -1, SEEK_CUR);
                            out->kind = T_IF;
                        } else {
                            fseek(state->stream, -1, SEEK_CUR);
                            parse_ident(state->stream, out);
                        }
                        break;
                    case 'n':
                        if (!cmp_string(state->stream, "t"))
                            out->kind = T_INT;
                        else
                            parse_ident(state->stream, out);
                        break;
                    default:
                        fseek(state->stream, -1, SEEK_CUR);
                        parse_ident(state->stream, out);
                }
                break;
            case 'l':
                if (!cmp_string(state->stream, "ong"))
                    out->kind = T_LONG;
                else
                    parse_ident(state->stream, out);
                break;
            case 'r':
                if (!cmp_string(state->stream, "eturn"))
                    out->kind = T_RETURN;
                else
                    parse_ident(state->stream, out);
                break;
            case 's':
                switch (next_char(state->stream)) {
                    case 'h':
                        if (!cmp_string(state->stream, "ort"))
                            out->kind = T_SHORT;
                        else
                            parse_ident(state->stream, out);
                        break;
                    case 'i':
                        switch (next_char(state->stream)) {
                            case 'g':
                                if (!cmp_string(state->stream, "ned"))
                                    out->kind = T_SIGNED;
                                else
                                    parse_ident(state->stream, out);
                                break;
                            case 'z':
                                if (!cmp_string(state->stream, "eof"))
                                    out->kind = T_SIZEOF;
                                else
                                    parse_ident(state->stream, out);
                                break;
                            default:
                                fseek(state->stream, -1, SEEK_CUR);
                                parse_ident(state->stream, out);
                        }
                        break;
                    case 't':
                        switch (next_char(state->stream)) {
                            case 'a':
                                if (!cmp_string(state->stream, "tic"))
                                    out->kind = T_STATIC;
                                else
                                    parse_ident(state->stream, out);
                                break;
                            case 'r':
                                if (!cmp_string(state->stream, "uct"))
                                    out->kind = T_STRUCT;
                                else
                                    parse_ident(state->stream, out);
                                break;
                            default:
                                fseek(state->stream, -1, SEEK_CUR);
                                parse_ident(state->stream, out);
                        }
                        break;
                    case 'w':
                        if (!cmp_string(state->stream, "itch"))
                            out->kind = T_SWITCH;
                        else
                            parse_ident(state->stream, out);
                        break;
                    default:
                        fseek(state->stream, -1, SEEK_CUR);
                        parse_ident(state->stream, out);
                }
                break;
            case 't':
                if (!cmp_string(state->stream, "ypedef"))
                    out->kind = T_TYPEDEF;
                else
                    parse_ident(state->stream, out);
                break;
            case 'u':
                if (next_char(state->stream) == 'n') {
                    switch (next_char(state->stream)) {
                        case 'i':
                            if (!cmp_string(state->stream, "on"))
                                out->kind = T_UNION;
                            else
                                parse_ident(state->stream, out);
                            break;
                        case 's':
                            if (!cmp_string(state->stream, "igned"))
                                out->kind = T_UNSIGNED;
                            else
                                parse_ident(state->stream, out);
                            break;
                        default:
                            fseek(state->stream, -1, SEEK_CUR);
                            parse_ident(state->stream, out);
                    }
                } else {
                    fseek(state->stream, -1, SEEK_CUR);
                    parse_ident(state->stream, out);
                }
                break;
            case 'v':
                if (next_char(state->stream) == 'o') {
                    switch (next_char(state->stream)) {
                        case 'i':
                            if (!cmp_string(state->stream, "d"))
                                out->kind = T_VOID;
                            else
                                parse_ident(state->stream, out);
                            break;
                        case 'l':
                            if (!cmp_string(state->stream, "atile"))
                                out->kind = T_VOLATILE;
                            else
                                parse_ident(state->stream, out);
                            break;
                        default:
                            fseek(state->stream, -1, SEEK_CUR);
                            parse_ident(state->stream, out);
                    }
                } else {
                    fseek(state->stream, -1, SEEK_CUR);
                    parse_ident(state->stream, out);
                }
                break;
            case 'w':
                if (!cmp_string(state->stream, "hile"))
                    out->kind = T_WHILE;
                else
                    parse_ident(state->stream, out);
                break;
            /* skip whitespace */
            case '\n':
                state->line ++;
            case ' ':
            case '\t':
                continue;
            default:
                if (isdigit(c)) {
                    parse_number(state->stream, out);
                } else if (isalpha(c) || c == '_') {
                    parse_ident(state->stream, out);
                } else if (c == '\'') {
                    /* parse a character literal */
                    state->current.kind = out->kind = T_CHAR_LIT;
                    state->current.file_start = ++ out->file_start;
                    if (!parse_single_char(state, '\''))
                        lex_error(state, "invalid char literal");
                    state->current.file_end = out->file_end =
                        ftell(state->stream);
                    if (!fread(&c, 1, 1, state->stream) || c != '\'')
                        lex_error(state, "expected terminating single quote");
                    return 1;
                } else if (c == '"') {
                    /* parse a string literal */
                    state->current.kind = out->kind = T_STRING_LIT;
                    state->current.file_start = ++ out->file_start;
                    while (parse_single_char(state, '"'));
                    state->current.file_end = out->file_end =
                        ftell(state->stream) - 1;
                    return 1;
                } else
                    /* unrecognized token */
                    lex_error(state, "unexpected token");
        }
        break;
    }

    state->current.kind = out->kind;
    state->current.file_start = out->file_start;
    state->current.file_end = out->file_end = ftell(state->stream);
    return 1;
}

void lex_rewind(struct lex_state *state) {
    state->repeat = 1;
}
