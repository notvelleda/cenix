%{
#include "as.h"
#include "y.tab.h"
%}

%%
hlt                 return HLT;
nop                 return NOP;
sf                  return SF;
rf                  return RF;
ei                  return EI;
di                  return DI;
sl                  return SL;
rl                  return RL;
cl                  return CL;
rsr                 return RSR;
ri                  return RI;
syn                 return SYN;
dly                 return DLY;
rsv                 return RSV;
bl                  return BL;
bnl                 return BNL;
bf                  return BF;
bnf                 return BNF;
bz                  return BZ;
bnz                 return BNZ;
bm                  return BM;
bp                  return BP;
bgz                 return BGZ;
ble                 return BLE;
bs1                 return BS1;
bs2                 return BS2;
bs3                 return BS3;
bs4                 return BS4;
bi                  return BI;
bck                 return BCK;
inr                 return INR;
dcr                 return DCR;
clr                 return CLR;
ivr                 return IVR;
srr                 return SRR;
slr                 return SLR;
rrr                 return RRR;
rlr                 return RLR;
add                 return ADD;
sub                 return SUB;
and                 return AND;
ori                 return ORI;
ore                 return ORE;
xfr                 return XFR;
jsys                return JSYS;
lst                 return LST;
sst                 return SST;
jmp                 return JMP;
mulu                return MULU;
divu                return DIVU;
jsr                 return JSR;
push                return PUSH;
pop                 return POP;
ld                  return LD;
st                  return ST;
ldm                 return LDM;
stm                 return STM;
lsm                 return LSM;
ssm                 return SSM;
flm                 return FLM;
u2e5                return U2E5;
lddmaa              return LDDMAA;
stdmaa              return STDMAA;
lddmac              return LDDMAC;
stdmac              return STDMAC;
stdmam              return STDMAM;
edma                return EDMA;
ddma                return DDMA;
ldmamap             return LDMAMAP;
sdmamap             return SDMAMAP;
addbig              return ADDBIG;
subbig              return SUBBIG;
cmpbig              return CMPBIG;
xfrbig              return XFRBIG;
negbig              return NEGBIG;
mulbig              return MULBIG;
divbig              return DIVBIG;
divbigr             return DIVBIGR;
ibaseconv           return IBASECONV;
obaseconv           return OBASECONV;
binld               return BINLD;
condcpy             return CONDCPY;
memcpy              return MEMCPY;
memcmp              return MEMCMP;
memset              return MEMSET;
mov16               return MOV16;


"%"aw               return REG_AW;
"%"au               return REG_AU;
"%"al               return REG_AL;
"%"bw               return REG_BW;
"%"bu               return REG_BU;
"%"bl               return REG_BL;
"%"xw               return REG_XW;
"%"xu               return REG_XU;
"%"xl               return REG_XL;
"%"yw               return REG_YW;
"%"yu               return REG_YU;
"%"yl               return REG_YL;
"%"zw               return REG_ZW;
"%"zu               return REG_ZU;
"%"zl               return REG_ZL;
"%"sw               return REG_SW;
"%"su               return REG_SU;
"%"sl               return REG_SL;
"%"cw               return REG_CW;
"%"cu               return REG_CU;
"%"cl               return REG_CL;
"%"pw               return REG_PW;
"%"pu               return REG_PU;
"%"pl               return REG_PL;

"-"                 return SYM_MINUS;
"--"                return SYM_DEC;

".byte"             return CONST_BYTE;
".short"            return CONST_SHORT;
".long"             return CONST_LONG;
".zero"             return CONST_ZERO;
".string"           return CONST_STRING;
".hstring"          return CONST_HSTRING;
".section"          return SECTION;
".global"           return GLOBAL;
".align"            return ALIGN;

"."[a-zA-Z0-9_-]+   { yylval.ptr = (void *) dup_str(yytext); return SECTION_NAME; }

"..="               return RANGE_INCL;
".."                return RANGE;

"0x"[0-9a-fA-F]+    { sscanf(yytext, "0x%lx", &yylval.num); return NUMBER; }
[0-9]+              { sscanf(yytext, "%ld", &yylval.num); return NUMBER; }

["][^"]*["]         { yylval.ptr = (void *) yytext; return STRING; }
['][^']*[']         { yylval.ptr = (void *) yytext; return STRING; }

[a-zA-Z0-9_-]+:     { yylval.ptr = (void *) yytext; return LABEL; }
[a-zA-Z0-9_-]+      { yylval.ptr_left = yylval.ptr; yylval.ptr = (void *) dup_str(yytext); return NAME; }

"$"                 return LITERAL;

"\n"                return EOL;
[ \t]+              return WHITESPACE;
"("                 return OPEN_PAREN;
")"                 return CLOSE_PAREN;
","                 return COMMA;
"+"                 return SYM_PLUS;
"++"                return SYM_INC;
"*"                 return SYM_MUL;
"/"                 return SYM_DIV;
"|"                 return SYM_OR;
"&"                 return SYM_AND;
"^"                 return SYM_XOR;
"~"                 return SYM_NEG;
"<<"                return SYM_SHL;
">>"                return SYM_SHR;
"#"                 return SYM_HASH;

%%
