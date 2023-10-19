#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "elf.h"

char *progname;
FILE *input;
FILE *output;
char is_little_endian;

void error(char *s) {
    fprintf(stderr, "%s: %s\n", progname, s);
    exit(1);
}

void check_ident(void) {
    unsigned char ident[16];
    fread(&ident, sizeof(ident), 1, input);

    if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 || ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
        error("invalid magic number");

    if (ident[EI_CLASS] != 1)
        error("input file must be a 32 bit ELF");

    if (ident[EI_DATA] != 2)
        error("only big-endian ELF files are supported");

    if (ident[EI_VERSION] != 1)
        error("only ELF version 1 is supported");
}

char read_byte(void) {
    char n;
    fread(&n, 1, 1, input);
    return n;
}

void write_byte(char c) {
    fwrite(&c, 1, 1, output);
}

uint16_t read_short(void) {
    uint16_t n;
    if (is_little_endian) {
        fread(((void *) &n) + 1, 1, 1, input);
        fread((void *) &n, 1, 1, input);
    } else
        fread(&n, 2, 1, input);
    return n;
}

void write_short(short n) {
    if (is_little_endian) {
        fwrite(((void *) &n) + 1, 1, 1, output);
        fwrite((void *) &n, 1, 1, output);
    } else
        fwrite(&n, 2, 1, output);
}

uint32_t read_long(void) {
    uint32_t n;
    if (is_little_endian) {
        fread(((void *) &n) + 3, 1, 1, input);
        fread(((void *) &n) + 2, 1, 1, input);
        fread(((void *) &n) + 1, 1, 1, input);
        fread((void *) &n, 1, 1, input);
    } else
        fread(&n, 4, 1, input);
    return n;
}

int main(int argc, char **argv) {
    short test = 1;
    long sh_offset;
    short sh_size;
    short num_sh;
    short shstrtab;
    Elf32_Shdr *shdrs;
    long *sec_offs;
    int i, j;
    short start_addr = 0x1000;

    progname = argv[0];

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <input> [output]\n", progname);
        return 1;
    }

    is_little_endian = *((char *) &test);

    input = fopen(argv[1], "r");

    if (argc == 3)
        output = fopen(argv[2], "w");
    else
        output = fopen("a.out", "w");

    check_ident();

    if (read_short() != ET_REL)
        error("input files must be relocatable");

    /* read useful things from header */
    fseek(input, 16 + 2 * 2 + 3 * 4, SEEK_SET);
    sh_offset = read_long();
    fseek(input, 4 + 3 * 2, SEEK_CUR);
    sh_size = read_short();
    num_sh = read_short();
    shstrtab = read_short();

    /* read section headers */
    fseek(input, sh_offset, SEEK_SET);
    shdrs = (Elf32_Shdr *) malloc(sizeof(Elf32_Shdr) * num_sh);
    sec_offs = (long *) malloc(sizeof(long) * num_sh);
    for (i = 0; i < num_sh; i ++) {
        shdrs[i].sh_name = read_long();
        shdrs[i].sh_type = read_long();
        shdrs[i].sh_flags = read_long();
        shdrs[i].sh_addr = read_long();
        shdrs[i].sh_offset = read_long();
        shdrs[i].sh_size = read_long();
        shdrs[i].sh_link = read_long();
        shdrs[i].sh_info = read_long();
        shdrs[i].sh_addralign = read_long();
        shdrs[i].sh_entsize = read_long();
        sec_offs[i] = ~0;
    }

    for (i = 0; i < num_sh; i ++) {
        Elf32_Shdr section = shdrs[i];

        if (section.sh_type == SHT_PROGBITS) {
            /* write section to output file */

            char *buf;
            long size;

            fseek(input, section.sh_offset, SEEK_SET);

            if (section.sh_addralign > 1) {
                long align = section.sh_addralign - 1;
                fseek(output, (ftell(output) + align) & ~align, SEEK_SET);
            }

            sec_offs[i] = ftell(output);

            buf = malloc(section.sh_size > 1024 ? 1024 : section.sh_size);

            if (buf == NULL)
                error("out of memory");

            for (size = section.sh_size; size > 1024; size -= 1024) {
                fread(buf, 1024, 1, input);
                fwrite(buf, 1024, 1, output);
            }

            fread(buf, size, 1, input);
            fwrite(buf, size, 1, output);

            free(buf);
        } else if (section.sh_type == SHT_RELA) {
            /* handle relocations */

            if (sec_offs[section.sh_info] == ~0)
                error("haven't written section yet!");

            for (j = 0; j < section.sh_size; j += section.sh_entsize) {
                Elf32_Shdr symtab;
                long offset, info, addend, sym_name, sym_offset, sym_section, dist;
                short sym_addr;

                /* read relocation entry */
                fseek(input, section.sh_offset + j, SEEK_SET);
                offset = read_long();
                info = read_long();
                addend = read_long();

                symtab = shdrs[section.sh_link];
                fseek(input, symtab.sh_offset + ELF32_R_SYM(info) * symtab.sh_entsize, SEEK_SET);
                sym_name = read_long();
                sym_offset = read_long();
                fseek(input, 6, SEEK_CUR);
                sym_section = read_short();

                if (sym_section == SHN_UNDEF) {
                    char c;
                    fprintf(stderr, "%s: undefined symbol ", progname);
                    fseek(input, shdrs[symtab.sh_link].sh_offset + sym_name, SEEK_SET);
                    for (fread(&c, 1, 1, input); c != 0; fread(&c, 1, 1, input))
                        fwrite(&c, 1, 1, stderr);
                    fprintf(stderr, "\n");
                    exit(1);
                }

                sym_addr = start_addr + (short) sec_offs[sym_section] + sym_offset + addend;

                fseek(output, sec_offs[section.sh_info] + offset, SEEK_SET);

                switch (ELF32_R_TYPE(info)) {
                    case R_CEN_NORMAL:
                        write_short(sym_addr);
                        break;
                    case R_CEN_PCREL:
                        dist = sym_addr - (ftell(output) + start_addr);
                        if (dist > 127 || dist < -128)
                            error("symbol is too far away");
                        write_byte((char) dist);
                        break;
                    default:
                        error("invalid relocation type");
                        break;
                }
            }
        }
    }

    return 0;
}
