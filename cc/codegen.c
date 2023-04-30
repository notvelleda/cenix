#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"

/* a, b, x, y, z (s, c, p are reserved) */
#define NUM_REGISTERS 10
#define NUM_PAIRS (NUM_REGISTERS / 2)

struct register_use {
    struct node *contents;
    unsigned int last_assigned;
    unsigned char is_used;
};

const char *whole_reg_names[NUM_REGISTERS] = {
    "aw", "?", "bw", "?", "xw", "?", "yw", "?", "zw", "?"
};

const char *half_reg_names[NUM_REGISTERS * 2] = {
    "au", "al", "bu", "bl", "xu", "xl", "yu", "yl", "zu", "zl"
};

struct cpu_state {
    struct register_use registers[NUM_REGISTERS];
    unsigned int op_count;
};

void save_register(
    struct cpu_state *state,
    struct node *target,
    unsigned char reg_num,
    unsigned char size
) {
    struct register_use *reg = &state->registers[reg_num];

    if (reg->is_used)
        fprintf(stderr, "TODO: push register %d on stack\n", reg_num);
    else
        reg->is_used = 1;

    reg->last_assigned = state->op_count;
    reg->contents = target;
    target->reg_num = reg_num;
}

unsigned int find_register(
    struct cpu_state *state,
    struct node *target,
    unsigned char size
) {
    unsigned char i;

    if (size == 0 || size > 2) {
        fprintf(stderr, "invalid register size\n");
        exit(1);
    }

    for (i = 0; i < NUM_REGISTERS; i += size) {
        struct register_use *reg = &state->registers[i];

        if (!reg->is_used) {
            if (size == 2 && state->registers[i + 1].is_used)
                continue;

            reg->is_used = 1;
            reg->last_assigned = state->op_count;
            reg->contents = target;
            target->reg_num = i;
            return i;
        }
    }

#if 0
    unsigned int lowest = -1; /* two's complement means this is the max value */
    unsigned char which_lowest;

    /* only check for unused registers if there are any */
    if (used != (1 << NUM_REGISTERS) - 1)
        for (i = 0; i < NUM_REGISTERS; i ++, used >>= 1)
            if (!(used & 1)) {
                /* found unused register, mark it as used n stuff */
                struct register_use *reg = &state->registers[i];
                state->use_bitmap |= 1 << i;
            }

    /* no unused registers were found, find the one that's been sitting the
     * longest */
    /* TODO: also check in how many operations registers will be accessed */

    for (i = 0; i < NUM_REGISTERS; i ++) {
        struct register_use *reg = &state->registers[i];
        if (reg->last_assigned < lowest) {
            lowest = reg->last_assigned;
            which_lowest = i;
        }
    }

    save_whole_register(state, target, which_lowest);

    return which_lowest;
#endif
    fprintf(stderr, "TODO: find register to overwrite\n");

    return 0;
}

unsigned char consume_register(
    struct cpu_state *state,
    struct node *old,
    struct node *new
) {
    unsigned char reg_num;

    if (old->kind == N_LVALUE)
        old = old->deps.single;

    reg_num = old->reg_num;

    if (old->visits <= 1)
        state->registers[reg_num].is_used = 0;
    else
        old->visits --;

    if (new != NULL)
        save_register(state, new, reg_num, 2);

    return reg_num;
}

void emit_return(struct node *node) {
    unsigned int reg_num;

    if (node->kind == N_LVALUE)
        node = node->deps.single;

    reg_num = node->reg_num;

    /* TODO: non-paired register */
    if (reg_num != 0)
        printf("xfr %%%s, %%aw\n", whole_reg_names[reg_num]);

    printf("rsr\n");
}

void codegen(struct node *head) {
    struct cpu_state state;
    struct node *cur;
    unsigned int reg_num;

    for (reg_num = 0; reg_num < NUM_REGISTERS; reg_num ++)
        state.registers[reg_num].is_used = 0;

    state.registers[4].is_used = 1;
    state.registers[5].is_used = 1;

    for (cur = head; cur != NULL; cur = cur->sorted_next) {
        switch (cur->kind) {
            case N_LITERAL:
                reg_num = find_register(&state, cur, 2);
                printf(
                    "ld %%%s, $%d\n",
                    whole_reg_names[reg_num],
                    cur->data.literal
                );
                break;
            case N_CALL:
                /* TODO: save/restore all used registers */
                save_register(&state, cur, 0, 2);
                printf("jsr %s\n", cur->data.tag);
                break;

            case N_LVALUE:
                /* skip lvalue nodes, they're meaningless here */
                continue;
            case N_RETURN:
                /* TODO: unwind stack */
                emit_return(cur->deps.single);
                break;

            case N_MUL:
                printf(
                    "mulu %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_DIV:
                printf(
                    "divu %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_MOD:
            case N_ADD:
                printf(
                    "add %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_SUB:
                printf(
                    "sub %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_SHIFT_LEFT:
                fprintf(stderr, "shift left unimplemented\n", cur->kind);
                continue;
            case N_SHIFT_RIGHT:
                fprintf(stderr, "shift right unimplemented\n", cur->kind);
                continue;
            case N_LESS:
                fprintf(stderr, "less than unimplemented\n", cur->kind);
                continue;
            case N_GREATER:
                fprintf(stderr, "greater than unimplemented\n", cur->kind);
                continue;
            case N_LESS_EQ:
                fprintf(stderr, "less eq unimplemented\n", cur->kind);
                continue;
            case N_GREATER_EQ:
                fprintf(stderr, "greater eq unimplemented\n", cur->kind);
                continue;
            case N_EQUALS:
                fprintf(stderr, "eq unimplemented\n", cur->kind);
                continue;
            case N_NOT_EQ:
                fprintf(stderr, "not eq unimplemented\n", cur->kind);
                continue;
            case N_BITWISE_AND:
                printf(
                    "and %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_BITWISE_OR:
                printf(
                    "ori %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_BITWISE_XOR:
                printf(
                    "ore %%%s, %%%s\n",
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.left, NULL)
                    ],
                    whole_reg_names[
                        consume_register(&state, cur->deps.op.right, cur)
                    ]
                );
                break;
            case N_AND:
                fprintf(stderr, "logical and unimplemented\n", cur->kind);
                continue;
            case N_OR:
                fprintf(stderr, "logical or unimplemented\n", cur->kind);
                continue;
        }

        state.op_count ++;
    }
}
