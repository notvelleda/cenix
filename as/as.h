#ifndef AS_H
#define AS_H

#define YYSTYPE struct yys_type

void out_byte(char c);
void checked_out_byte(long n);
void checked_out_short(long n);
void checked_out_long(long n);
void out_string(char *s);
void out_hstring(char *s);

void start_program_section(char *s);
void align(long a);
void add_label(char *s);
void global_name(char *s);
void ref_name(char *s, char rel_type, long addend);

char * dup_strlit(char *s);
char * dup_str(char *s);

void yyerror(char *s);
int yylex(void);

struct yys_type {
    long num;
    long num_left;
    long num_left2;
    void *ptr;
    void *ptr_left;
    char reg;
    char reg_left;
    char reg_left2;
};

#endif
