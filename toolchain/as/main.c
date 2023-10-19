#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "as.h"
#include "y.tab.h"
#include "elf.h"

char *progname;
char *input_name;
FILE *output;
char is_little_endian;

void out_byte(char c) {
    fwrite(&c, 1, 1, output);
}

void checked_out_byte(long n) {
    if ((n & ~0xff) > 0)
        yyerror("value overflow");
    else
        out_byte((char) n);
}

void out_short(short n) {
    if (is_little_endian) {
        fwrite(((void *) &n) + 1, 1, 1, output);
        fwrite((void *) &n, 1, 1, output);
    } else
        fwrite(&n, 2, 1, output);
}

void checked_out_short(long n) {
    if ((n & ~0xffff) > 0)
        yyerror("value overflow");
    else
        out_short((short) n);
}

void out_long(long n) {
    if (is_little_endian) {
        fwrite(((void *) &n) + 3, 1, 1, output);
        fwrite(((void *) &n) + 2, 1, 1, output);
        fwrite(((void *) &n) + 1, 1, 1, output);
        fwrite((void *) &n, 1, 1, output);
    } else
        fwrite(&n, 4, 1, output);
}

void checked_out_long(long n) {
    if ((n & ~0xffffffff) > 0)
        yyerror("value overflow");
    else
        out_long((long) n);
}

void out_string(char *s) {
    char initial;
    for (initial = *(s ++); *s != initial; s ++)
        fwrite(s, 1, 1, output);
}

void out_hstring(char *s) {
    char initial;
    for (initial = *(s ++); *s != initial; s ++) {
        char c = (*s) | 0x80;
        fwrite(&c, 1, 1, output);
    }
}

char * dup_strlit(char *s) {
    char initial;
    char *orig = s = dup_str(s);
    for (initial = *(s ++); *s != initial; s ++) {}
    *s = 0;
    return orig + 1;
}

/* linked list is the wrong data structure for this but i'm too lazy to care */
struct name_entry {
    char *name;
    int index;
    int table_index;

    struct name_entry *next;
};

static struct name_entry *first_name_entry = (struct name_entry *) NULL;
static struct name_entry *last_name_entry = (struct name_entry *) NULL;
static int last_name_entry_index = 0;

static struct name_entry * find_name_entry(char *name, char can_free) {
    if (first_name_entry != NULL) {
        struct name_entry *current;

        /* try to find the given name in the name list */
        for (current = first_name_entry; current != NULL && strcmp(current->name, name) != 0; current = current->next) {}

        if (current != NULL) {
            if (can_free)
                free(name);
            return current;
        }
    }

    /* couldn't find the name, insert a new link into the name entry list and use it */
    if (last_name_entry == NULL)
        first_name_entry = last_name_entry = (struct name_entry *) malloc(sizeof(struct name_entry));
    else {
        struct name_entry *prev = last_name_entry;
        last_name_entry = (struct name_entry *) malloc(sizeof(struct name_entry));
        prev->next = last_name_entry;
    }

    if (last_name_entry == NULL)
        yyerror("out of memory");

    last_name_entry->name = name;
    last_name_entry->index = last_name_entry_index ++;
    last_name_entry->table_index = 0; /* to be populated later */
    last_name_entry->next = (struct name_entry *) NULL;

    return last_name_entry;
}

static struct name_entry * get_name(int index) {
    struct name_entry *current = first_name_entry;
    for (; current != NULL && current->index != index; current = current->next) {}
    return current;
}

struct section {
    int name_index;
    long offset;
    long size;
    long type;
    long flags;
    long link;
    long info;
    long align;
    long entsize;
    int rel_name_index;
    struct symbol_ref *first_symbol_ref;
    struct symbol_ref *last_symbol_ref;
    int index;

    struct section *next;
};

static struct section *first_section = (struct section *) NULL;
static struct section *last_section = (struct section *) NULL;
static int last_section_index = 1;

static struct section * get_section(int index) {
    struct section *current = first_section;
    for (; current != NULL && current->index != index; current = current->next) {}
    return current;
}

static struct section * start_section(int name_index, long type, long flags) {
    if (last_section == NULL)
        /* no elements in the linked list yet, add the first one */
        first_section = last_section = (struct section *) malloc(sizeof(struct section));
    else {
        /* there's already at least one element in the linked list, add another one to it */
        struct section *prev = last_section;
        last_section = (struct section *) malloc(sizeof(struct section));
        prev->size = ftell(output) - prev->offset;
        prev->next = last_section;
    }

    if (last_section == NULL)
        yyerror("out of memory");

    last_section->name_index = name_index;
    last_section->offset = ftell(output);
    last_section->size = 0; /* to be populated later */
    last_section->type = type;
    last_section->flags = flags;
    last_section->link = 0;
    last_section->info = 0;
    last_section->align = 0;
    last_section->entsize = 0;
    last_section->rel_name_index = 0;
    last_section->first_symbol_ref = (struct symbol_ref *) NULL;
    last_section->last_symbol_ref = (struct symbol_ref *) NULL;
    last_section->index = last_section_index ++;
    last_section->next = (struct section *) NULL;

    return last_section;
}

void start_program_section(char *s) {
    start_section(find_name_entry(s, 1)->index, SHT_PROGBITS, SHF_WRITE | SHF_EXECINSTR);
}

void align(long a) {
    /* TODO: allow aligning inside sections without affecting section alignment */

    if (last_section == NULL)
        yyerror(".align must come after a section definition");

    last_section->align = a;
}

struct symbol_def {
    int name_index;
    int section_index;
    long section_offset;
    char is_global;
    int index;

    struct symbol_def *next;
};

static struct symbol_def *first_symbol_def = (struct symbol_def *) NULL;
static struct symbol_def *last_symbol_def = (struct symbol_def *) NULL;
static int last_symbol_def_index = 1;

static struct symbol_def * get_symbol(int index) {
    struct symbol_def *current = first_symbol_def;
    for (; current != NULL && current->index != index; current = current->next) {}
    return current;
}

static struct symbol_def * find_existing_symbol(int name_index) {
    struct symbol_def *current;
    for (current = first_symbol_def; current != NULL && current->name_index != name_index; current = current->next) {}
    return current;
}

static struct symbol_def * find_symbol(int name_index, int section_index, long section_offset) {
    if (first_symbol_def != NULL) {
        struct symbol_def *current;

        for (current = first_symbol_def; current != NULL && current->name_index != name_index; current = current->next) {}

        if (current != NULL) {
            if (section_index != SHN_UNDEF && current->section_index == SHN_UNDEF) {
                current->section_index = section_index;
                current->section_offset = section_offset;
            }
            return current;
        }
    }

    if (last_symbol_def == NULL)
        first_symbol_def = last_symbol_def = (struct symbol_def *) malloc(sizeof(struct symbol_def));
    else {
        struct symbol_def *prev = last_symbol_def;
        last_symbol_def = (struct symbol_def *) malloc(sizeof(struct symbol_def));
        prev->next = last_symbol_def;
    }

    if (last_symbol_def == NULL)
        yyerror("out of memory");

    last_symbol_def->name_index = name_index;
    last_symbol_def->section_index = section_index;
    last_symbol_def->section_offset = section_offset;
    last_symbol_def->is_global = 0;
    last_symbol_def->index = last_symbol_def_index ++;
    last_symbol_def->next = (struct symbol_def *) NULL;

    return last_symbol_def;
}

void add_label(char *s) {
    int len;
    char *ptr;
    struct name_entry *name;

    /* duplicate the string, remove the : from the end */
    len = strlen(s);
    ptr = malloc(len);
    memcpy(ptr, s, len);
    *(ptr + len - 1) = 0;

    /* register this symbol */
    find_symbol(find_name_entry(ptr, 1)->index, last_section->index, ftell(output) - last_section->offset);
}

void global_name(char *s) {
    struct symbol_def *sym = find_existing_symbol(find_name_entry(s, 1)->index);

    if (sym == NULL)
        yyerror("symbol doesn't exist");

    sym->is_global = 1;
}

struct symbol_ref {
    int symbol_index;
    short section_index;
    long section_offset;
    char rel_type;
    long addend;

    struct symbol_ref *next;
};

void ref_name(char *s, char rel_type, long addend) {
    struct symbol_def *symbol;
    struct symbol_ref *ref;

    if (last_section->last_symbol_ref == NULL)
        ref = last_section->first_symbol_ref = last_section->last_symbol_ref = (struct symbol_ref *) malloc(sizeof(struct symbol_ref));
    else {
        struct symbol_ref *prev = last_section->last_symbol_ref;
        ref = last_section->last_symbol_ref = (struct symbol_ref *) malloc(sizeof(struct symbol_ref));
        prev->next = last_section->last_symbol_ref;
    }    

    if (ref == NULL)
        yyerror("out of memory");

    /* register this symbol */
    symbol = find_symbol(find_name_entry(s, 1)->index, SHN_UNDEF, 0);

    ref->symbol_index = symbol->index;
    ref->section_index = last_section->index;
    ref->section_offset = ftell(output) - last_section->offset;
    ref->rel_type = rel_type;
    ref->addend = addend;
    ref->next = (struct symbol_ref *) NULL;
}

char * dup_str(char *s) {
    int len;
    char *ptr;

    len = strlen(s) + 1;
    ptr = malloc(len);
    memcpy(ptr, s, len);
    *(ptr + len - 1) = 0;

    return ptr;
}

void yyerror(char *s) {
    extern long line;
    fprintf(stderr, "%s: %s:%d: %s\n", progname, input_name, line, s);
    exit(1);
}

int yywrap(void) {
    return 1;
}

static const unsigned char ident[16] = {
    /* magic numbers */ ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
    /* class (32 bit) */ 1,
    /* endianness (big-endian) */ 2,
    /* elf version */ 1,
    /* os abi */ 0xe6,
    /* os abi version */ 0,
    /* padding */ 0, 0, 0, 0, 0, 0, 0,
};

static void write_header(void) {
    fwrite(&ident, sizeof(ident), 1, output);
    out_short(ET_REL); /* e_type */
    out_short(0); /* e_machine */
    out_long(1); /* e_version */
    out_long(0); /* e_entry */
    out_long(0); /* e_phoff */
    out_long(0); /* e_shoff */
    out_long(0); /* e_flags */
    out_short(sizeof(ident) + 8 * 2 + 5 * 4); /* e_ehsize */
    out_short(0); /* e_phentsize */
    out_short(0); /* e_phnum */
    out_short(10 * 4); /* e_shentsize */
    out_short(0); /* e_shnum */
    out_short(0); /* e_shstrndx */
}

static void write_sections(void) {
    long sh_start;
    long num_sh;
    struct section *sections;
    struct name_entry *names = first_name_entry;
    struct symbol_def *symbols = first_symbol_def;
    struct symbol_ref *relocations;
    struct section *symtab_section;
    struct section *strtab_section;
    struct section *rel_section;
    int symtab_name_index;
    int size = 1;

    /* === make sure reserved names are in the string table === */
    symtab_name_index = find_name_entry(".symtab", 0)->index;
    for (sections = first_section; sections != NULL; sections = sections->next)
        if (sections->first_symbol_ref != NULL) {
            char *name = get_name(sections->name_index)->name;
            int len = strlen(name) + 6;
            char *ptr = (char *) malloc(len);
            sprintf(ptr, ".rela%s", name);
            sections->rel_name_index = find_name_entry(ptr, 1)->index;
        }


    /* === write string table === */
    strtab_section = start_section(find_name_entry(".strtab", 0)->index, SHT_STRTAB, 0);

    out_byte(0);

    for (; names != NULL; names = names->next) {
        names->table_index = size;
        int len = strlen(names->name) + 1;
        fwrite(names->name, len, 1, output);
        size += len;
    }

    /* === write symbol table === */
    symtab_section = start_section(symtab_name_index, SHT_SYMTAB, 0);
    symtab_section->link = strtab_section->index;
    symtab_section->info = 1; /* symbol count */
    symtab_section->entsize = 3 * 4 + 1 * 2 + 2;

    /* write null symbol */
    out_long(0); /* st_name */
    out_long(0); /* st_value */
    out_long(0); /* st_size */
    out_byte(0); /* st_info */
    out_byte(0); /* st_other */
    out_short(0); /* st_shndx */

    for (; symbols != NULL; symbols = symbols->next, symtab_section->info ++) {
        out_long(get_name(symbols->name_index)->table_index); /* st_name */
        out_long(symbols->section_offset); /* st_value */
        out_long(0); /* st_size */
        out_byte(ELF32_ST_INFO(symbols->is_global ? STB_GLOBAL : STB_LOCAL, STT_NOTYPE)); /* st_info */
        out_byte(ELF32_ST_VISIBILITY(STV_DEFAULT)); /* st_other */
        switch (symbols->section_index) {
            case SHN_UNDEF:
            case SHN_ABS:
            case SHN_COMMON:
                out_short(symbols->section_index); /* st_shndx */
                break;
            default:
                out_short((short) get_section(symbols->section_index)->index); /* st_shndx */
                break;
        }
    }

    /* === write relocation tables === */
    for (sections = first_section; sections != NULL; sections = sections->next) {
        if (sections->first_symbol_ref != NULL) {
            rel_section = start_section(sections->rel_name_index, SHT_RELA, 0);
            rel_section->link = symtab_section->index;
            rel_section->info = sections->index;
            rel_section->entsize = 3 * 4;

            for (relocations = sections->first_symbol_ref; relocations != NULL; relocations = relocations->next) {
                out_long(relocations->section_offset);
                out_long(ELF32_R_INFO(relocations->symbol_index, relocations->rel_type));
                out_long(relocations->addend);
            }
        }
    }

    /* === set last section's size properly === */
    last_section->size = ftell(output) - last_section->offset;

    /* === write section headers === */
    num_sh = last_section_index;
    sh_start = ftell(output);

    /* write null section */
    out_long(0); /* sh_name */
    out_long(0); /* sh_type */
    out_long(0); /* sh_flags */
    out_long(0); /* sh_addr */
    out_long(0); /* sh_offset */
    out_long(0); /* sh_size */
    out_long(0); /* sh_link */
    out_long(0); /* sh_info */
    out_long(0); /* sh_addralign */
    out_long(0); /* sh_entsize */

    /* write sections */
    for (sections = first_section; sections != NULL; sections = sections->next) {
        out_long(get_name(sections->name_index)->table_index); /* sh_name */
        out_long(sections->type); /* sh_type */
        out_long(sections->flags); /* sh_flags */
        out_long(0); /* sh_addr */
        out_long(sections->offset); /* sh_offset */
        out_long(sections->size); /* sh_size */
        out_long(sections->link); /* sh_link */
        out_long(sections->info); /* sh_info */
        out_long(sections->align); /* sh_addralign */
        out_long(sections->entsize); /* sh_entsize */
    }

    /* fix header fields */
    fseek(output, sizeof(ident) + 2 * 2 + 3 * 4, SEEK_SET);
    out_long(sh_start);
    fseek(output, sizeof(ident) + 6 * 2 + 5 * 4, SEEK_SET);
    out_short(num_sh);
    fseek(output, sizeof(ident) + 7 * 2 + 5 * 4, SEEK_SET);
    out_short(strtab_section->index);
    fseek(output, 0, SEEK_END);
}

int main(int argc, char **argv) {
    extern FILE *yyin;
    short test = 1;

    progname = argv[0];

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <input> [output]\n", progname);
        return 1;
    }

    is_little_endian = *((char *) &test);

    if (argv[1][0] == '-' && argv[1][2] == 0) {
        input_name = "<stdin>";
        yyin = stdin;
    } else {
        input_name = argv[1];
        yyin = fopen(argv[1], "r");
    }

    if (argc == 3)
        output = fopen(argv[2], "w");
    else
        output = fopen("a.out", "w");

    write_header();
    yyparse();
    write_sections();

    return 0;
}
