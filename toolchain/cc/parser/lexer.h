#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <sys/types.h>

/* list of all kinds of tokens we understand */
enum token_kind {
    /* an identifier/keyword/type/etc */
    T_IDENT,
    /* a base-10 number */
    T_NUMBER,
    /* a base-16 number */
    T_HEX_NUMBER,
    /* a base-8 number :skull: */
    T_OCT_NUMBER,
    /* a string literal in double quotes */
    T_STRING_LIT,
    /* a single quoted character literal */
    T_CHAR_LIT,
    /* an opening parenthesis */
    T_OPEN_PAREN,
    /* a closing parenthesis */
    T_CLOSE_PAREN,
    /* an opening square bracket */
    T_OPEN_BRACKET,
    /* a closing square bracket */
    T_CLOSE_BRACKET,
    /* an opening curly bracket */
    T_OPEN_CURLY,
    /* a closing curly bracket */
    T_CLOSE_CURLY,
    /* a semicolon, used to end statements */
    T_SEMICOLON,
    /* a colon, used to end switch case statements */
    T_COLON,
    /* a comma, used to separate arguments */
    T_COMMA,
    /* an asterisk, used to signify types as pointers, dereference
     * pointers, or multiply */
    T_ASTERISK,
    /* an ampersand, used to create a pointer to something or as the
     * bitwise and operator */
    T_AMPERSAND,
    /* the plus operator (add) */
    T_PLUS,
    /* the increment operator (++) */
    T_INCREMENT,
    /* the decrement operator (--) */
    T_DECREMENT,
    /* the minus operator (negate/subtract) */
    T_MINUS,
    /* the period operator, used to access fields in structs */
    T_PERIOD,
    /* the arrow operator, used to access fields in struct pointers (->) */
    T_ARROW,
    /* the assign operator (=) */
    T_ASSIGN,
    /* the equals operator (==) */
    T_EQUALS,
    /* the not equal operator (!=) */
    T_NOT_EQ,
    /* the not operator (!) */
    T_LOGICAL_NOT,
    /* the division operator (/) */
    T_DIVIDE,
    /* the modulo operator (%) */
    T_MODULO,
    /* the less than operator (<) */
    T_LESS_THAN,
    /* the less than or equal operator (<=) */
    T_LESS_EQ,
    /* the greater than operator (>) */
    T_GREATER_THAN,
    /* the greater than or equal operator (>=) */
    T_GREATER_EQ,
    /* the logical and operator (&&) */
    T_LOGICAL_AND,
    /* the logical or operator (||) */
    T_LOGICAL_OR,
    /* the bitwise or operator (|) */
    T_BITWISE_OR,
    /* the bitwise xor operator (^) */
    T_BITWISE_XOR,
    /* the bitwise not operator (~) */
    T_BITWISE_NOT,
    /* bitwise left shift operator (<<) */
    T_LEFT_SHIFT,
    /* bitwise right shift operator (>>) */
    T_RIGHT_SHIFT,
    /* ternary operator (?) */
    T_TERNARY,
/* === reserved keywords === */
    T_BREAK,
    T_CASE,
    T_CHAR,
    T_CONST,
    T_CONTINUE,
    T_DEFAULT,
    T_DO,
    T_ELSE,
    T_ENUM,
    T_EXTERN,
    T_FOR,
    T_IF,
    T_INT,
    T_LONG,
    T_RETURN,
    T_SHORT,
    T_SIGNED,
    T_SIZEOF,
    T_STATIC,
    T_STRUCT,
    T_SWITCH,
    T_TYPEDEF,
    T_UNION,
    T_UNSIGNED,
    T_VOID,
    T_VOLATILE,
    T_WHILE,
};

/* a single token outputted by the lexer */
struct token {
    /* what kind of token this is */
    enum token_kind kind;
    /* start of the token in the file */
    off_t file_start;
    /* end of the token in the file */
    off_t file_end;
    /* the line number this token is located on */
    uint16_t line;
};

/* the state of the lexer */
struct lex_state {
    /* the stream currently being lexed */
    FILE *stream;
    /* the line number the lexer is on, useful for error messages */
    uint16_t line;
    /* the name of the file being lexed */
    char *filename;
    /* whether the current token should be repeated */
    char repeat;
    /* the current token */
    struct token current;
};

/* prints out an error message with the current filename and line number, then
 * exits */
void lex_error(struct lex_state *state, const char *message);

/* parses one token from the provided file, returning 1 when there are more
 * tokens to be parsed in the file, and 0 if there aren't. if 0 is returned,
 * the state of the output token is undefined */
char lex(struct lex_state *state, struct token *out);

/* tells the lexer to repeat the current token */
void lex_rewind(struct lex_state *state);

/* given a token, read the area in the file it points to into a newly allocated
 * null terminated string and return it */
char *token_to_string(struct lex_state *state, struct token *t);

/* given a token, read the area in the file it points to and parse it into a
 * number of the given base, up to base 16 */
/* TODO: add binary notation (0b...) support since it'll be easy */
uint32_t token_to_number(
    struct lex_state *state,
    struct token *t,
    uint32_t base
);

#endif
