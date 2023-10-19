/* probably the world's worst c compiler */

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "hashtable.h"
#include "output.h"

int main(int argc, char **argv) {
    /*struct token the_token;*/
    struct lex_state state;
    FILE *output_file;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }

    state.line = 1;
    state.filename = basename(argv[1]);
    if (!(state.stream = fopen(argv[1], "r"))) {
        fprintf(stderr, "failed to open %s\n", argv[1]);
        return 1;
    }
    if (!(output_file = fopen(argv[2], "w"))) {
        fprintf(stderr, "failed to open %s\n", argv[1]);
        return 1;
    }
    /*while (lex(&state, &the_token))
        printf("%d: %d - %d\n", the_token.kind, the_token.file_start, the_token.file_end);*/
    set_output_file(output_file);
    parse(&state);
    fclose(state.stream);

#if 0
    struct node *current, *current2, *last;

    /* owo() ^ 5 */
    current = malloc(sizeof(struct node));
    current->kind = N_CALL;
    current->data.tag = "owo";
    current2 = malloc(sizeof(struct node));
    current2->kind = N_LITERAL;
    current2->data.literal = 5;
    last = malloc(sizeof(struct node));
    last->kind = N_XOR;
    last->edges.op.left = current;
    last->edges.op.right = current2;

    /* foo = owo() ^ 5 */
    current = malloc(sizeof(struct node));
    current->kind = N_STORE;
    current->edges.single = last;
    current->data.tag = "foo";
    last = current;

    debug_graph(last);
#endif

#if 0
    struct hashtable *table = (struct hashtable *) malloc(sizeof(struct hashtable));
    char *test;
    if (table == NULL) {
        perror("wtf");
        return 1;
    }
    hashtable_init(table);

    if (!hashtable_insert(table, "this is a very long string", (void *) "hiiii"))
        printf("aww\n");
    if (!hashtable_insert(table, "owo", (void *) ":3c"))
        printf("aww\n");
    if (!hashtable_insert(table, "uwu", (void *) ":3"))
        printf("aww\n");
    if (!hashtable_insert(table, "uwu", (void *) "4:"))
        printf("aww\n");

    if ((test = (char *) hashtable_lookup(table, "uwu")) == NULL) {
        printf("not there\n");
    } else {
        printf("got %s\n", test);
    }
    if ((test = (char *) hashtable_lookup(table, "this is a very long string")) == NULL) {
        printf("not there\n");
    } else {
        printf("got %s\n", test);
    }
    if ((test = (char *) hashtable_lookup(table, "owo")) == NULL) {
        printf("not there\n");
    } else {
        printf("got %s\n", test);
    }
    if ((test = (char *) hashtable_lookup(table, ":3")) == NULL) {
        printf("not there\n");
    } else {
        printf("got %s\n", test);
    }

    hashtable_free(table, 0);
    free(table);
#endif

    return 0;
}
