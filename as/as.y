%{
#include <stdio.h>
#include <math.h>
#include "as.h"
#include "elf.h"

#define REG_AW_NIBBLE 0x0
#define REG_AU_NIBBLE 0x0
#define REG_AL_NIBBLE 0x1
#define REG_BW_NIBBLE 0x2
#define REG_BU_NIBBLE 0x2
#define REG_BL_NIBBLE 0x3
#define REG_XW_NIBBLE 0x4
#define REG_XU_NIBBLE 0x4
#define REG_XL_NIBBLE 0x5
#define REG_YW_NIBBLE 0x6
#define REG_YU_NIBBLE 0x6
#define REG_YL_NIBBLE 0x7
#define REG_ZW_NIBBLE 0x8
#define REG_ZU_NIBBLE 0x8
#define REG_ZL_NIBBLE 0x9
#define REG_SW_NIBBLE 0xa
#define REG_SU_NIBBLE 0xa
#define REG_SL_NIBBLE 0xb
#define REG_CW_NIBBLE 0xc
#define REG_CU_NIBBLE 0xc
#define REG_CL_NIBBLE 0xd
#define REG_PW_NIBBLE 0xe
#define REG_PU_NIBBLE 0xe
#define REG_PL_NIBBLE 0xf

#define REG_MOD_NORMAL 0x0
#define REG_MOD_INC 0x1
#define REG_MOD_DEC 0x2
#define REG_MOD_DEREF 0x4
#define REG_MOD_ADD 0x8

#define LDST_KIND_ADDR 0
#define LDST_KIND_NAME 1
#define LDST_KIND_DEREF_ADDR 2
#define LDST_KIND_DEREF_NAME 3
#define LDST_KIND_DEREF_REG 4
#define LDST_KIND_DEREF_REG_ADD 5
#define LDST_KIND_LIT 6
#define LDST_KIND_LIT_NAME 7

#define LDST_BYTE (0 << 6)
#define LDST_WORD (1 << 6)

#define LDST_LOAD (0 << 7)
#define LDST_STORE (1 << 7)

#define LDST_OP_LIT 0
#define LDST_OP_ADDR 1
#define LDST_OP_DEREF 2
#define LDST_OP_DEREF_REG 5

static void ldst(unsigned char);

static void xfr_byte(void);
static void xfr_word(void);

static void write_2e_mode_1_left(void);
static void write_2e_mode_1(void);

static void inc_byte_reg(void);
static void dec_byte_reg(void);
static void clr_byte_reg(void);
static void ivr_byte_reg(void);
static void srr_byte_reg(void);
static void slr_byte_reg(void);

static void inc_word_reg(void);
static void dec_word_reg(void);
static void clr_word_reg(void);
static void ivr_word_reg(void);
static void srr_word_reg(void);
static void slr_word_reg(void);

static void add_byte_regs(void);
static void sub_byte_regs(void);
static void and_byte_regs(void);
static void add_word_regs(void);
static void sub_word_regs(void);
static void and_word_regs(void);

static void out_reg_const(void);
static void write_reg_range(char, char);
static void write_reg_range_incl(char, char);

static unsigned char opcode;
static unsigned char reg_kind;
long line = 1;
extern char *input_name;
long math_stack[256];
unsigned char math_stack_pos = 0;
%}

/* opcodes */
%token HLT NOP SF RF EI DI SL RL CL RSR RI SYN DLY RSV BL BNL BF
%token BNF BZ BNZ BM BP BGZ BLE BS1 BS2 BS3 BS4 BI BCK INR DCR CLR IVR SRR SLR
%token RRR RLR ADD SUB AND ORI ORE XFR JSYS LST SST JMP MULU DIVU JSR PUSH
%token POP LD ST LDM STM LSM SSM FLM U2E5 LDDMAA STDMAA LDDMAC STDMAC STDMAM
%token EDMA DDMA LDMAMAP SDMAMAP ADDBIG SUBBIG CMPBIG XFRBIG NEGBIG MULBIG
%token DIVBIG DIVBIGR IBASECONV OBASECONV BINLD CONDCPY MEMCPY MEMCMP MEMSET
%token MOV16

/* register names */
%token REG_AW REG_AU REG_AL REG_BW REG_BU REG_BL REG_XW REG_XU REG_XL REG_YW
%token REG_YU REG_YL REG_ZW REG_ZU REG_ZL REG_SW REG_SU REG_SL REG_CW REG_CU
%token REG_CL REG_PW REG_PU REG_PL

%token LABEL LITERAL NAME SECTION_NAME NUMBER STRING

%token YYEOF EOL WHITESPACE COMMA PERIOD OPEN_PAREN CLOSE_PAREN RANGE
%token RANGE_INCL SYM_MINUS SYM_DEC SYM_PLUS SYM_INC SYM_MUL SYM_DIV SYM_OR
%token SYM_AND SYM_XOR SYM_NEG SYM_SHL SYM_SHR SYM_HASH

%token CONST_BYTE CONST_SHORT CONST_LONG CONST_ZERO CONST_STRING CONST_HSTRING
%token SECTION GLOBAL ALIGN

%%
prog:
    | prog opt_whitespace end_inst
    | prog WHITESPACE inst end_inst
    | prog label opt_whitespace end_inst
    | prog label WHITESPACE inst end_inst
    | prog WHITESPACE SECTION WHITESPACE SECTION_NAME end_inst { start_program_section((char *) yylval.ptr); }
    | prog WHITESPACE GLOBAL WHITESPACE NAME end_inst { global_name((char *) yylval.ptr); }
    | prog WHITESPACE ALIGN WHITESPACE NUMBER end_inst { align(yylval.num); }
    | prog SYM_HASH WHITESPACE simple_num_left2 WHITESPACE STRING thing_idk EOL { input_name = dup_strlit((char *) yylval.ptr); line = yylval.num_left2; }
    ;

thing_idk:
    | WHITESPACE NUMBER
    | WHITESPACE NUMBER WHITESPACE NUMBER WHITESPACE NUMBER
    ;

end_inst: EOL { line ++; }
    | YYEOF { line ++; }
    ;

opt_whitespace:
    | WHITESPACE
    ;

simple_num_left2: NUMBER { yylval.num_left2 = yylval.num; }
    ;

simple_num_left: NUMBER { math_stack[math_stack_pos ++] = yylval.num; }
    ;

complex_num: NUMBER
    | simple_num_left opt_whitespace SYM_PLUS opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] + yylval.num; }
    | simple_num_left opt_whitespace SYM_MINUS opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] - yylval.num; }
    | simple_num_left opt_whitespace SYM_MUL opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] * yylval.num; }
    | simple_num_left opt_whitespace SYM_DIV opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] / yylval.num; }
    | simple_num_left opt_whitespace SYM_AND opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] & yylval.num; }
    | simple_num_left opt_whitespace SYM_OR opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] | yylval.num; }
    | simple_num_left opt_whitespace SYM_XOR opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] ^ yylval.num; }
    | SYM_NEG opt_whitespace complex_num { yylval.num = ~yylval.num; }
    | simple_num_left opt_whitespace SYM_SHL opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] << yylval.num; }
    | simple_num_left opt_whitespace SYM_SHR opt_whitespace complex_num { yylval.num = math_stack[-- math_stack_pos] >> yylval.num; }
    ;

complex_num_left: complex_num { yylval.num_left = yylval.num; }
    ;

complex_num_left2: complex_num { yylval.num_left2 = yylval.num; }
    ;

literal_num: LITERAL complex_num
    ;

literal_num_left: LITERAL complex_num_left
    ;

literal_name: LITERAL NAME
    ;

label: LABEL { add_label(yylval.ptr); }
    ;

comma: COMMA opt_whitespace
    ;

byte_const: complex_num { checked_out_byte(yylval.num); }
    | byte_const comma complex_num { checked_out_byte(yylval.num); }
    ;

short_const: complex_num { checked_out_short(yylval.num); }
    | NAME { ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | short_const comma complex_num { checked_out_short(yylval.num); }
    | short_const comma NAME { ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    ;

long_const: complex_num { checked_out_long(yylval.num); }
    | long_const comma complex_num { checked_out_long(yylval.num); }
    ;

string_const: STRING { out_string((char *) yylval.ptr); }
    | string_const comma STRING { out_string((char *) yylval.ptr); }
    ;

hstring_const: STRING { out_hstring((char *) yylval.ptr); }
    | hstring_const comma STRING { out_hstring((char *) yylval.ptr); }
    ;

byte_register: REG_AU { yylval.reg = REG_AU_NIBBLE; }
    | REG_AL { yylval.reg = REG_AL_NIBBLE; }
    | REG_BU { yylval.reg = REG_BU_NIBBLE; }
    | REG_BL { yylval.reg = REG_BL_NIBBLE; }
    | REG_XU { yylval.reg = REG_XU_NIBBLE; }
    | REG_XL { yylval.reg = REG_XL_NIBBLE; }
    | REG_YU { yylval.reg = REG_YU_NIBBLE; }
    | REG_YL { yylval.reg = REG_YL_NIBBLE; }
    | REG_ZU { yylval.reg = REG_ZU_NIBBLE; }
    | REG_ZL { yylval.reg = REG_ZL_NIBBLE; }
    | REG_SU { yylval.reg = REG_SU_NIBBLE; }
    | REG_SL { yylval.reg = REG_SL_NIBBLE; }
    | REG_CU { yylval.reg = REG_CU_NIBBLE; }
    | REG_CL { yylval.reg = REG_CL_NIBBLE; }
    | REG_PU { yylval.reg = REG_PU_NIBBLE; }
    | REG_PL { yylval.reg = REG_PL_NIBBLE; }
    ;

byte_register_left: byte_register { yylval.reg_left = yylval.reg; }
    ;

word_register: REG_AW { yylval.reg = REG_AW_NIBBLE; }
    | REG_BW { yylval.reg = REG_BW_NIBBLE; }
    | REG_XW { yylval.reg = REG_XW_NIBBLE; }
    | REG_YW { yylval.reg = REG_YW_NIBBLE; }
    | REG_ZW { yylval.reg = REG_ZW_NIBBLE; }
    | REG_SW { yylval.reg = REG_SW_NIBBLE; }
    | REG_CW { yylval.reg = REG_CW_NIBBLE; }
    | REG_PW { yylval.reg = REG_PW_NIBBLE; }
    ;

word_register_left: word_register { yylval.reg_left = yylval.reg; }
    ;

open_paren: OPEN_PAREN opt_whitespace
    ;

close_paren: CLOSE_PAREN opt_whitespace
    ;

inc: SYM_INC opt_whitespace
    ;

dec: SYM_DEC opt_whitespace
    ;

deref_reg: open_paren word_register opt_whitespace close_paren
    ;

word_reg_mod: word_register opt_whitespace { yylval.reg = yylval.reg << 4 | REG_MOD_NORMAL; }
    | word_register opt_whitespace inc { yylval.reg = yylval.reg << 4 | REG_MOD_INC; }
    | dec word_register opt_whitespace { yylval.reg = yylval.reg << 4 | REG_MOD_DEC; }
    | deref_reg { yylval.reg = yylval.reg << 4 | REG_MOD_DEREF | REG_MOD_NORMAL; }
    | open_paren word_register opt_whitespace inc close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_DEREF | REG_MOD_INC; }
    | open_paren dec word_register opt_whitespace close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_DEREF | REG_MOD_DEC; }
    ;

word_reg_add: complex_num opt_whitespace open_paren word_register opt_whitespace opt_whitespace close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_NORMAL; }
    | complex_num opt_whitespace open_paren word_register opt_whitespace inc opt_whitespace close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_INC; }
    | complex_num opt_whitespace open_paren dec word_register opt_whitespace opt_whitespace close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_DEC; }
    | open_paren complex_num opt_whitespace open_paren word_register opt_whitespace opt_whitespace close_paren close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_DEREF | REG_MOD_NORMAL; }
    | open_paren complex_num opt_whitespace open_paren word_register opt_whitespace inc opt_whitespace close_paren close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_DEREF | REG_MOD_INC; }
    | open_paren complex_num opt_whitespace open_paren dec word_register opt_whitespace opt_whitespace close_paren close_paren { yylval.reg = yylval.reg << 4 | REG_MOD_ADD | REG_MOD_DEREF | REG_MOD_DEC; }
    ;

jmp_op: JMP { opcode = 0x71; }
    ;
jsr_op: JSR { opcode = 0x79; }
    ;

jump: complex_num { out_byte(opcode); checked_out_short(yylval.num); }
    | NAME { out_byte(opcode); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | open_paren complex_num opt_whitespace CLOSE_PAREN { out_byte(opcode + 1); checked_out_short(yylval.num); }
    | open_paren NAME opt_whitespace CLOSE_PAREN { out_byte(opcode + 1); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | open_paren word_reg_mod CLOSE_PAREN { out_byte(opcode + 4); out_byte(yylval.reg); }
    | word_reg_add { out_byte(opcode + 4); out_byte(yylval.reg); checked_out_byte(yylval.num); }

push_op: PUSH { opcode = 0x7e; }
    ;
pop_op: POP { opcode = 0x7f; }
    ;

push_pop: byte_register { out_byte(opcode); out_byte(yylval.reg << 4); }
    | word_register { out_byte(opcode); out_byte(yylval.reg << 4 | 0x1); }
    | byte_register_left opt_whitespace RANGE opt_whitespace byte_register { out_byte(opcode); write_reg_range(0, 0); }
    | byte_register_left opt_whitespace RANGE opt_whitespace word_register { out_byte(opcode); write_reg_range(0, 1); }
    | word_register_left opt_whitespace RANGE opt_whitespace byte_register { out_byte(opcode); write_reg_range(1, 0); }
    | word_register_left opt_whitespace RANGE opt_whitespace word_register { out_byte(opcode); write_reg_range(1, 1); }
    | byte_register_left opt_whitespace RANGE_INCL opt_whitespace byte_register { out_byte(opcode); write_reg_range_incl(0, 0); }
    | byte_register_left opt_whitespace RANGE_INCL opt_whitespace word_register { out_byte(opcode); write_reg_range_incl(0, 1); }
    | word_register_left opt_whitespace RANGE_INCL opt_whitespace byte_register { out_byte(opcode); write_reg_range_incl(1, 0); }
    | word_register_left opt_whitespace RANGE_INCL opt_whitespace word_register { out_byte(opcode); write_reg_range_incl(1, 1); }
    | RANGE opt_whitespace byte_register { yylval.reg_left = REG_AW_NIBBLE; out_byte(opcode); write_reg_range(0, 0); }
    | RANGE opt_whitespace word_register { yylval.reg_left = REG_AW_NIBBLE; out_byte(opcode); write_reg_range(0, 1); }
    | RANGE_INCL opt_whitespace byte_register { yylval.reg_left = REG_AW_NIBBLE; out_byte(opcode); write_reg_range_incl(0, 0); }
    | RANGE_INCL opt_whitespace word_register { yylval.reg_left = REG_AW_NIBBLE; out_byte(opcode); write_reg_range_incl(0, 1); }
    | byte_register_left opt_whitespace RANGE { yylval.reg = 0xf; out_byte(opcode); write_reg_range_incl(0, 0); }
    | word_register_left opt_whitespace RANGE { yylval.reg = 0xf; out_byte(opcode); write_reg_range_incl(1, 0); }
    | RANGE { out_byte(opcode); out_byte(0x0f); }

ldm_op: LDM { opcode = 0; }
    ;
stm_op: STM { opcode = 1; }
    ;
lsm_op: LSM { opcode = 2; }
    ;
ssm_op: SSM { opcode = 3; }
    ;
flm_op: FLM { opcode = 4; }
    ;
u2e5_op: U2E5 { opcode = 5; }
    ;

deref_reg_offsl: complex_num_left2 opt_whitespace open_paren word_register opt_whitespace close_paren { yylval.reg_left2 = (yylval.reg << 4) | REG_AW_NIBBLE; }
    | complex_num_left2 opt_whitespace open_paren word_register_left opt_whitespace SYM_PLUS opt_whitespace word_register close_paren { yylval.reg_left2 = yylval.reg_left | yylval.reg << 4; }
    | open_paren word_register_left opt_whitespace SYM_PLUS opt_whitespace word_register close_paren { yylval.num_left2 = 0; yylval.reg_left2 = yylval.reg_left | yylval.reg << 4; }
    ;

deref_reg_offsr: complex_num opt_whitespace open_paren word_register opt_whitespace close_paren { yylval.reg = (yylval.reg << 4) | REG_AW_NIBBLE; }
    | complex_num opt_whitespace open_paren word_register_left opt_whitespace SYM_PLUS opt_whitespace word_register close_paren { yylval.reg |= yylval.reg_left << 4; }
    | open_paren word_register_left opt_whitespace SYM_PLUS opt_whitespace word_register close_paren { yylval.num = 0; yylval.reg |= yylval.reg_left << 4; }
    ;

op_ext_lbyte_num_left: complex_num { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 0); checked_out_short(yylval.num_left); checked_out_short(yylval.num); }
    | NAME { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 0); checked_out_short(yylval.num_left); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | deref_reg_offsr { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 1); checked_out_short(yylval.num_left); write_2e_mode_1(); }
    | deref_reg { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 2); checked_out_short(yylval.num_left); out_byte(yylval.reg << 4 | yylval.reg); }
    | literal_num { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 3); checked_out_short(yylval.num_left); checked_out_byte(yylval.num); }
    ;

op_ext_lbyte_name_left: complex_num { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 0); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); checked_out_short(yylval.num); }
    | NAME { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 0); ref_name((char *) yylval.ptr_left, R_CEN_NORMAL, 0); out_short(0x0000); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | deref_reg_offsr { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 1); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); write_2e_mode_1(); }
    | deref_reg { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 2); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); out_byte(yylval.reg << 4 | yylval.reg); }
    | literal_num { out_byte(0x2e); out_byte(opcode << 4 | 0 << 2 | 3); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); checked_out_byte(yylval.num); }
    ;

opt_ext_lbyte_roffs_left: complex_num { out_byte(0x2e); out_byte(opcode << 4 | 1 << 2 | 0); write_2e_mode_1_left(); checked_out_short(yylval.num); }
    | NAME { out_byte(0x2e); out_byte(opcode << 4 | 1 << 2 | 0); write_2e_mode_1_left(); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | deref_reg_offsr { out_byte(0x2e); out_byte(opcode << 4 | 1 << 2 | 1); write_2e_mode_1_left(); write_2e_mode_1(); }
    | deref_reg { out_byte(0x2e); out_byte(opcode << 4 | 1 << 2 | 2); write_2e_mode_1_left(); out_byte(yylval.reg << 4 | yylval.reg); }
    | literal_num { out_byte(0x2e); out_byte(opcode << 4 | 1 << 2 | 3); write_2e_mode_1_left(); checked_out_byte(yylval.num); }
    ;

op_ext_lbyte_reg_left: complex_num { out_byte(0x2e); out_byte(opcode << 4 | 2 << 2 | 0); out_byte(yylval.reg_left << 4 | yylval.reg_left); checked_out_short(yylval.num); }
    | NAME { out_byte(0x2e); out_byte(opcode << 4 | 2 << 2 | 0); out_byte(yylval.reg_left << 4 | yylval.reg_left); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | deref_reg_offsr { out_byte(0x2e); out_byte(opcode << 4 | 2 << 2 | 1); out_byte(yylval.reg_left << 4 | yylval.reg_left); write_2e_mode_1(); }
    | deref_reg { out_byte(0x2e); out_byte(opcode << 4 | 2 << 2 | 2); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | literal_num { out_byte(0x2e); out_byte(opcode << 4 | 2 << 2 | 3); out_byte(yylval.reg_left << 4 | yylval.reg_left); checked_out_byte(yylval.num); }
    ;

op_ext_lbyte_lit_left: complex_num { out_byte(0x2e); out_byte(opcode << 4 | 3 << 2 | 0); checked_out_byte(yylval.num_left); checked_out_short(yylval.num); }
    | NAME { out_byte(0x2e); out_byte(opcode << 4 | 3 << 2 | 0); checked_out_byte(yylval.num_left); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | deref_reg_offsr { out_byte(0x2e); out_byte(opcode << 4 | 3 << 2 | 1); checked_out_byte(yylval.num_left); write_2e_mode_1(); }
    | deref_reg { out_byte(0x2e); out_byte(opcode << 4 | 3 << 2 | 2); checked_out_byte(yylval.num_left); out_byte(yylval.reg << 4 | yylval.reg); }
    | literal_num { out_byte(0x2e); out_byte(opcode << 4 | 3 << 2 | 3); checked_out_byte(yylval.num_left); checked_out_byte(yylval.num); }

op_extaddr_lbyte: complex_num_left comma op_ext_lbyte_num_left
    | NAME comma op_ext_lbyte_name_left
    | deref_reg_offsl comma opt_ext_lbyte_roffs_left
    | open_paren word_register_left opt_whitespace close_paren comma op_ext_lbyte_reg_left
    | literal_num_left comma op_ext_lbyte_lit_left
    ;

ld_byte_reg: LD WHITESPACE byte_register_left opt_whitespace comma { reg_kind = LDST_BYTE | LDST_LOAD; }
    ;
ld_word_reg: LD WHITESPACE word_register_left opt_whitespace comma { reg_kind = LDST_WORD | LDST_LOAD; }
    ;
st_byte_reg: ST WHITESPACE byte_register_left opt_whitespace comma { reg_kind = LDST_BYTE | LDST_STORE; }
    ;
st_word_reg: ST WHITESPACE word_register_left opt_whitespace comma { reg_kind = LDST_WORD | LDST_STORE; }
    ;

ldst: literal_num { ldst(LDST_KIND_LIT | reg_kind); }
    | literal_name { ldst(LDST_KIND_LIT_NAME | reg_kind); }
    | complex_num { ldst(LDST_KIND_ADDR | reg_kind); }
    | NAME { ldst(LDST_KIND_NAME | reg_kind); }
    | open_paren complex_num opt_whitespace CLOSE_PAREN { ldst(LDST_KIND_DEREF_ADDR | reg_kind); }
    | open_paren NAME opt_whitespace CLOSE_PAREN { ldst(LDST_KIND_DEREF_NAME | reg_kind); }
    | open_paren word_reg_mod CLOSE_PAREN { ldst(LDST_KIND_DEREF_REG | reg_kind); }
    | word_reg_add { ldst(LDST_KIND_DEREF_REG_ADD | reg_kind); }
    ;

inst: CONST_BYTE WHITESPACE byte_const
    | CONST_SHORT WHITESPACE short_const
    | CONST_LONG WHITESPACE long_const
    | CONST_STRING WHITESPACE string_const
    | CONST_HSTRING WHITESPACE hstring_const
    | CONST_ZERO WHITESPACE complex_num { long i = 0; for (; i < yylval.num; i ++) out_byte(0); }
    | HLT { out_byte(0x00); }
    | NOP { out_byte(0x01); }
    | SF { out_byte(0x02); }
    | RF { out_byte(0x03); }
    | EI { out_byte(0x04); }
    | DI { out_byte(0x05); }
    | SL { out_byte(0x06); }
    | RL { out_byte(0x07); }
    | CL { out_byte(0x08); }
    | RSR { out_byte(0x09); }
    | RI { out_byte(0x0a); }
    | SYN { out_byte(0x0c); }
    | DLY { out_byte(0x0e); }
    | RSV { out_byte(0x0f); }
    | BL WHITESPACE NAME { out_byte(0x10); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BNL WHITESPACE NAME { out_byte(0x11); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BF WHITESPACE NAME { out_byte(0x12); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BNF WHITESPACE NAME { out_byte(0x13); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BZ WHITESPACE NAME { out_byte(0x14); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BNZ WHITESPACE NAME { out_byte(0x15); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BM WHITESPACE NAME { out_byte(0x16); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BP WHITESPACE NAME { out_byte(0x17); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BGZ WHITESPACE NAME { out_byte(0x18); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BLE WHITESPACE NAME { out_byte(0x19); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BS1 WHITESPACE NAME { out_byte(0x1a); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BS2 WHITESPACE NAME { out_byte(0x1b); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BS3 WHITESPACE NAME { out_byte(0x1c); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BS4 WHITESPACE NAME { out_byte(0x1d); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BI WHITESPACE NAME { out_byte(0x1e); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | BCK WHITESPACE NAME { out_byte(0x1f); ref_name((char *) yylval.ptr, R_CEN_PCREL, -1); out_byte(0x00); }
    | INR WHITESPACE byte_register { inc_byte_reg(); }
    | INR WHITESPACE byte_register comma complex_num { out_byte(0x20); yylval.num --; out_reg_const(); }
    | DCR WHITESPACE byte_register { dec_byte_reg(); }
    | DCR WHITESPACE byte_register comma complex_num { out_byte(0x21); yylval.num --; out_reg_const(); }
    | CLR WHITESPACE byte_register { clr_byte_reg(); }
    | CLR WHITESPACE byte_register comma complex_num { out_byte(0x22); out_reg_const(); }
    | IVR WHITESPACE byte_register { ivr_byte_reg(); }
    | IVR WHITESPACE byte_register comma complex_num { out_byte(0x23); out_reg_const(); }
    | SRR WHITESPACE byte_register { srr_byte_reg(); }
    | SRR WHITESPACE byte_register comma complex_num { out_byte(0x24); yylval.num --; out_reg_const(); }
    | SLR WHITESPACE byte_register { slr_byte_reg(); }
    | SLR WHITESPACE byte_register comma complex_num { out_byte(0x25); yylval.num --; out_reg_const(); }
    | RRR WHITESPACE byte_register { out_byte(0x26); out_byte(yylval.reg << 4); }
    | RRR WHITESPACE byte_register comma complex_num { out_byte(0x26); yylval.num --; out_reg_const(); }
    | RLR WHITESPACE byte_register { out_byte(0x27); out_byte(yylval.reg << 4); }
    | RLR WHITESPACE byte_register comma complex_num { out_byte(0x27); yylval.num --; out_reg_const(); }
    | ldm_op WHITESPACE op_extaddr_lbyte
    | stm_op WHITESPACE op_extaddr_lbyte
    | lsm_op WHITESPACE op_extaddr_lbyte
    | ssm_op WHITESPACE op_extaddr_lbyte
    | flm_op WHITESPACE op_extaddr_lbyte
    | u2e5_op WHITESPACE op_extaddr_lbyte
    | LDDMAA WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x0); }
    | STDMAA WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x1); }
    | LDDMAC WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x2); }
    | STDMAC WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x3); }
    | STDMAM WHITESPACE byte_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x4); }
    | STDMAM WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x5); }
    | EDMA { out_byte(0x2f); out_byte(0x6); }
    | DDMA { out_byte(0x2f); out_byte(0x7); }
    | LDMAMAP WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x8); }
    | SDMAMAP WHITESPACE word_register { out_byte(0x2f); out_byte(yylval.reg << 4 | 0x9); }
    | INR WHITESPACE word_register { inc_word_reg(); }
    | INR WHITESPACE word_register comma complex_num { out_byte(0x30); yylval.num --; out_reg_const(); }
    | DCR WHITESPACE word_register { dec_word_reg(); }
    | DCR WHITESPACE word_register comma complex_num { out_byte(0x31); yylval.num --; out_reg_const(); }
    | CLR WHITESPACE word_register { clr_word_reg(); }
    | CLR WHITESPACE word_register comma complex_num { out_byte(0x32); out_reg_const(); }
    | IVR WHITESPACE word_register { ivr_word_reg(); }
    | IVR WHITESPACE word_register comma complex_num { out_byte(0x33); out_reg_const(); }
    | SRR WHITESPACE word_register { srr_word_reg(); }
    | SRR WHITESPACE word_register comma complex_num { out_byte(0x34); yylval.num --; out_reg_const(); }
    | SLR WHITESPACE word_register { slr_word_reg(); }
    | SLR WHITESPACE word_register comma complex_num { out_byte(0x35); yylval.num --; out_reg_const(); }
    | RRR WHITESPACE word_register { out_byte(0x36); out_byte(yylval.reg << 4); }
    | RRR WHITESPACE word_register comma complex_num { out_byte(0x36); yylval.num --; out_reg_const(); }
    | RLR WHITESPACE word_register { out_byte(0x37); out_byte(yylval.reg << 4); }
    | RLR WHITESPACE word_register comma complex_num { out_byte(0x37); yylval.num --; out_reg_const(); }
    | ADD WHITESPACE byte_register_left comma byte_register { add_byte_regs(); }
    | SUB WHITESPACE byte_register_left comma byte_register { sub_byte_regs(); }
    | AND WHITESPACE byte_register_left comma byte_register { and_byte_regs(); }
    | ORI WHITESPACE byte_register_left comma byte_register { out_byte(0x43); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | ORE WHITESPACE byte_register_left comma byte_register { out_byte(0x44); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | XFR WHITESPACE byte_register_left comma byte_register { xfr_byte(); }
    | ADD WHITESPACE word_register_left comma word_register { add_word_regs(); }
    | SUB WHITESPACE word_register_left comma word_register { sub_word_regs(); }
    | AND WHITESPACE word_register_left comma word_register { and_word_regs(); }
    | ORI WHITESPACE word_register_left comma word_register { out_byte(0x53); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | ORE WHITESPACE word_register_left comma word_register { out_byte(0x54); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | XFR WHITESPACE word_register_left comma word_register { xfr_word(); }
    | JSYS WHITESPACE literal_num { out_byte(0x66); checked_out_byte(yylval.num); }
    | jmp_op WHITESPACE jump
    | MULU WHITESPACE word_register_left comma word_register { out_byte(0x77); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | DIVU WHITESPACE word_register_left comma word_register { out_byte(0x78); out_byte(yylval.reg_left << 4 | yylval.reg); }
    | jsr_op WHITESPACE jump
    | push_op WHITESPACE push_pop
    | pop_op WHITESPACE push_pop
    | ld_byte_reg ldst
    | ld_word_reg ldst
    | st_byte_reg ldst
    | st_word_reg ldst
    | LST WHITESPACE complex_num { out_byte(0x6e); checked_out_short(yylval.num); }
    | LST WHITESPACE NAME { out_byte(0x6e); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    | SST WHITESPACE complex_num { out_byte(0x6f); checked_out_short(yylval.num); }
    | SST WHITESPACE NAME { out_byte(0x6f); ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0); out_short(0x0000); }
    ;
%%

static unsigned char ldst_opcode(unsigned char kind) {
    switch (kind & 0xc0) {
        case LDST_WORD | LDST_LOAD:
            switch (yylval.reg_left) {
                case REG_AW_NIBBLE:
                    return 0x90; /* LDAW */
                case REG_BW_NIBBLE:
                    return 0xd0; /* LDBW */
                case REG_XW_NIBBLE:
                    return 0x60; /* LDXW */
                default:
                    yyerror("unsupported register");
            }
        case LDST_WORD | LDST_STORE:
            switch (yylval.reg_left) {
                case REG_AW_NIBBLE:
                    return 0xb0; /* STAW */
                case REG_BW_NIBBLE:
                    return 0xf0; /* STBW */
                case REG_XW_NIBBLE:
                    return 0x68; /* STXW */
                default:
                    yyerror("unsupported register");
            }
        case LDST_BYTE | LDST_LOAD:
            switch (yylval.reg_left) {
                case REG_AL_NIBBLE:
                    return 0x80; /* LDAL */
                case REG_BL_NIBBLE:
                    return 0xc0; /* LDBL */
                default:
                    yyerror("unsupported register");
            }
        case LDST_BYTE | LDST_STORE:
            switch (yylval.reg_left) {
                case REG_AL_NIBBLE:
                    return 0xa0; /* STAL */
                case REG_BL_NIBBLE:
                    return 0xe0; /* STBL */
                default:
                    yyerror("unsupported register");
            }
    }
}

static void ldst(unsigned char kind) {
    char tmp;

    switch (kind) {
        case LDST_KIND_LIT | LDST_BYTE | LDST_LOAD: /* load literal into byte register */
            out_byte(ldst_opcode(kind) + LDST_OP_LIT);
            checked_out_byte(yylval.num);
            break;
        case LDST_KIND_LIT | LDST_WORD | LDST_LOAD: /* load literal into word register */
            out_byte(ldst_opcode(kind) + LDST_OP_LIT);
            checked_out_short(yylval.num);
            break;
        case LDST_KIND_LIT_NAME | LDST_WORD | LDST_LOAD: /* load address of name into word register */
            out_byte(ldst_opcode(kind) + LDST_OP_LIT);
            ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0);
            out_short(0x0000);
            break;
        case LDST_KIND_ADDR | LDST_WORD | LDST_LOAD: /* load word at static address into word register */
        case LDST_KIND_ADDR | LDST_WORD | LDST_STORE: /* store word register at static address */
        case LDST_KIND_ADDR | LDST_BYTE | LDST_LOAD:
        case LDST_KIND_ADDR | LDST_BYTE | LDST_STORE:
            out_byte(ldst_opcode(kind) + LDST_OP_ADDR);
            checked_out_short(yylval.num);
            break;
        case LDST_KIND_NAME | LDST_WORD | LDST_LOAD: /* load word at name into word register */
        case LDST_KIND_NAME | LDST_WORD | LDST_STORE: /* store word register at name */
        case LDST_KIND_NAME | LDST_BYTE | LDST_LOAD:
        case LDST_KIND_NAME | LDST_BYTE | LDST_STORE:
            out_byte(ldst_opcode(kind) + LDST_OP_ADDR);
            ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0);
            out_short(0x0000);
            break;
        case LDST_KIND_DEREF_ADDR | LDST_WORD | LDST_LOAD: /* deref word at static address, loading into word register */
        case LDST_KIND_DEREF_ADDR | LDST_WORD | LDST_STORE: /* deref word at static address, storing word register */
        case LDST_KIND_DEREF_ADDR | LDST_BYTE | LDST_LOAD:
        case LDST_KIND_DEREF_ADDR | LDST_BYTE | LDST_STORE:
            out_byte(ldst_opcode(kind) + LDST_OP_DEREF);
            checked_out_short(yylval.num);
            break;
        case LDST_KIND_DEREF_NAME | LDST_WORD | LDST_LOAD: /* deref word at name, loading into word register */
        case LDST_KIND_DEREF_NAME | LDST_WORD | LDST_STORE: /* deref word at name, storing word register */
        case LDST_KIND_DEREF_NAME | LDST_BYTE | LDST_LOAD:
        case LDST_KIND_DEREF_NAME | LDST_BYTE | LDST_STORE:
            out_byte(ldst_opcode(kind) + LDST_OP_DEREF);
            ref_name((char *) yylval.ptr, R_CEN_NORMAL, 0);
            out_short(0x0000);
            break;
        case LDST_KIND_DEREF_REG | LDST_WORD | LDST_LOAD: /* deref register, loading into word register */
            if (yylval.reg & 0xf == REG_MOD_NORMAL)
                if (yylval.reg_left == REG_AW_NIBBLE) {
                    out_byte(0x98 | (yylval.reg >> 5)); /* LAWA..LAWP */
                    return;
                } else if (yylval.reg_left == REG_BW_NIBBLE) {
                    out_byte(0xd8 | (yylval.reg >> 5)); /* LBWA..LBWP */
                    return;
                }
            goto deref_ref_normal;
        case LDST_KIND_DEREF_REG | LDST_WORD | LDST_STORE: /* deref register, storing word register */
            if (yylval.reg & 0xf == REG_MOD_NORMAL)
                if (yylval.reg_left == REG_AW_NIBBLE) {
                    out_byte(0xb8 | (yylval.reg >> 5)); /* SAWA..SAWP */
                    return;
                } else if (yylval.reg_left == REG_BW_NIBBLE) {
                    out_byte(0xf8 | (yylval.reg >> 5)); /* SBWA..SBWP */
                    return;
                }
            goto deref_ref_normal;
        case LDST_KIND_DEREF_REG | LDST_BYTE | LDST_LOAD:
            if (yylval.reg & 0xf == REG_MOD_NORMAL)
                if (yylval.reg_left == REG_AL_NIBBLE) {
                    out_byte(0x88 | (yylval.reg >> 5)); /* LALA..LALP */
                    return;
                } else if (yylval.reg_left == REG_BL_NIBBLE) {
                    out_byte(0xc8 | (yylval.reg >> 5)); /* LBLA..LBLP */
                    return;
                }
            goto deref_ref_normal;
        case LDST_KIND_DEREF_REG | LDST_BYTE | LDST_STORE:
            if (yylval.reg & 0xf == REG_MOD_NORMAL)
                if (yylval.reg_left == REG_AL_NIBBLE) {
                    out_byte(0xa8 | (yylval.reg >> 5)); /* SALA..SALP */
                    return;
                } else if (yylval.reg_left == REG_BL_NIBBLE) {
                    out_byte(0xe8 | (yylval.reg >> 5)); /* SBLA..SBLP */
                    return;
                }
        deref_ref_normal:
            out_byte(ldst_opcode(kind) + LDST_OP_DEREF_REG);
            out_byte(yylval.reg);
            break;
        case LDST_KIND_DEREF_REG_ADD | LDST_WORD | LDST_LOAD: /* deref register with addend, loading into word register */
        case LDST_KIND_DEREF_REG_ADD | LDST_WORD | LDST_STORE: /* deref register with addend, storing word register */
        case LDST_KIND_DEREF_REG_ADD | LDST_BYTE | LDST_LOAD:
        case LDST_KIND_DEREF_REG_ADD | LDST_BYTE | LDST_STORE:
            out_byte(ldst_opcode(kind) + LDST_OP_DEREF_REG);
            out_byte(yylval.reg);
            checked_out_byte(yylval.num);
            break;
        default:
            yyerror("invalid load/store");
            break;
    }
}

static void xfr_byte(void) {
    if (yylval.reg_left == REG_AL_NIBBLE)
        switch (yylval.reg) {
            case REG_XL_NIBBLE:
                out_byte(0x48); /* XAXL */
                return;
            case REG_YL_NIBBLE:
                out_byte(0x49); /* XAYL */
                return;
            case REG_BL_NIBBLE:
                out_byte(0x4a); /* XABL */
                return;
            case REG_ZL_NIBBLE:
                out_byte(0x4b); /* XAZL */
                return;
            case REG_SL_NIBBLE:
                out_byte(0x4c); /* XASL */
                return;
        }

    out_byte(0x45); /* XFR (byte form) */
    out_byte(yylval.reg_left << 4 | yylval.reg);
}

static void xfr_word(void) {
    if (yylval.reg_left == REG_AW_NIBBLE)
        switch (yylval.reg) {
            case REG_XW_NIBBLE:
                out_byte(0x5b); /* XAXW */
                return;
            case REG_YW_NIBBLE:
                out_byte(0x5c); /* XAYW */
                return;
            case REG_BW_NIBBLE:
                out_byte(0x5d); /* XABW */
                return;
            case REG_ZW_NIBBLE:
                out_byte(0x5e); /* XAZW */
                return;
            case REG_SW_NIBBLE:
                out_byte(0x5f); /* XASW */
                return;
        }
    else if (yylval.reg_left == REG_PW_NIBBLE && yylval.reg == REG_XW_NIBBLE) {
        out_byte(0x0d); /* PCX */
        return;
    }

    out_byte(0x55); /* XFR (word form) */
    out_byte(yylval.reg_left << 4 | yylval.reg);
}

static void write_2e_mode_1_left(void) {
    if ((yylval.num_left2 & ~0xff) > 0)
        yylval.reg_left2 |= 1 << 5; /* make r1 odd */

    out_byte(yylval.reg_left2);

    if ((yylval.num_left2 & ~0xff) > 0)
        checked_out_short(yylval.num_left2);
    else
        out_byte(yylval.num_left2);
}

static void write_2e_mode_1(void) {
    if ((yylval.num & ~0xff) > 0)
        yylval.reg |= 1 << 5; /* make r1 odd */

    out_byte(yylval.reg);

    if ((yylval.num & ~0xff) > 0)
        checked_out_short(yylval.num);
    else
        out_byte(yylval.num);
}

static void inc_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x28); /* INAL */
    else {
        out_byte(0x20); /* INR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void dec_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x29); /* DCAL */
    else {
        out_byte(0x21); /* DCR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void clr_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x2a); /* CLAL */
    else {
        out_byte(0x22); /* CLR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void ivr_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x2b); /* IVAL */
    else {
        out_byte(0x23); /* IVR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void srr_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x2c); /* SRAL */
    else {
        out_byte(0x24); /* SRR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void slr_byte_reg(void) {
    if (yylval.reg == REG_AL_NIBBLE)
        out_byte(0x2d); /* SLAL */
    else {
        out_byte(0x25); /* SLR (byte reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void inc_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x38); /* INAW */
    else if (yylval.reg == REG_XW_NIBBLE)
        out_byte(0x3e); /* INX */
    else {
        out_byte(0x30); /* INR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void dec_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x39); /* DCAW */
    else if (yylval.reg == REG_XW_NIBBLE)
        out_byte(0x3f); /* DCX */
    else {
        out_byte(0x31); /* DCR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void clr_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x3a); /* CLAW */
    else {
        out_byte(0x32); /* CLR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void ivr_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x3b); /* IVAW */
    else {
        out_byte(0x33); /* IVR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void srr_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x3c); /* SRAW */
    else {
        out_byte(0x34); /* SRR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void slr_word_reg(void) {
    if (yylval.reg == REG_AW_NIBBLE)
        out_byte(0x3d); /* SLAW */
    else {
        out_byte(0x35); /* SLR (word reg form) */
        out_byte(yylval.reg << 4);
    }
}

static void add_byte_regs(void) {
    if (yylval.reg_left == REG_AL_NIBBLE && yylval.reg == REG_BL_NIBBLE)
        out_byte(0x48); /* AABL */
    else {
        out_byte(0x40); /* ADD (byte reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void sub_byte_regs(void) {
    if (yylval.reg_left == REG_AL_NIBBLE && yylval.reg == REG_BL_NIBBLE)
        out_byte(0x49); /* SABL */
    else {
        out_byte(0x41); /* SUB (byte reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void and_byte_regs(void) {
    if (yylval.reg_left == REG_AL_NIBBLE && yylval.reg == REG_BL_NIBBLE)
        out_byte(0x4a); /* NABL */
    else {
        out_byte(0x42); /* AND (byte reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void add_word_regs(void) {
    if (yylval.reg_left == REG_AW_NIBBLE && yylval.reg == REG_BW_NIBBLE)
        out_byte(0x58); /* AABW */
    else {
        out_byte(0x50); /* ADD (word reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void sub_word_regs(void) {
    if (yylval.reg_left == REG_AW_NIBBLE && yylval.reg == REG_BW_NIBBLE)
        out_byte(0x59); /* SABW */
    else {
        out_byte(0x51); /* SUB (word reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void and_word_regs(void) {
    if (yylval.reg_left == REG_AW_NIBBLE && yylval.reg == REG_BW_NIBBLE)
        out_byte(0x5a); /* NABW */
    else {
        out_byte(0x52); /* AND (word reg form) */
        out_byte(yylval.reg_left << 4 | yylval.reg);
    }
}

static void out_reg_const(void) {
    if ((yylval.num & ~0xf) != 0)
        yyerror("constant overflow");

    out_byte(yylval.reg << 4 | (unsigned char) yylval.num);
}

static void write_reg_range(char is_start_word, char is_end_word) {
    char count;

    if (yylval.reg_left > yylval.reg || (is_start_word && yylval.reg_left + 1 > yylval.reg) || (is_end_word && yylval.reg_left > yylval.reg + 1))
        yyerror("ending value of range must be greater than starting value");

    count = yylval.reg - yylval.reg_left - 1;

    if ((count & ~0xf) != 0)
        yyerror("range overflow");

    out_byte(yylval.reg_left << 4 | count);
}

static void write_reg_range_incl(char is_start_word, char is_end_word) {
    if (is_end_word)
        yylval.reg += 2;
    else
        yylval.reg ++;

    write_reg_range(is_start_word, is_end_word);
}
