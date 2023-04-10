#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define EI_MAG0 0x00
#define EI_MAG1 0x01
#define EI_MAG2 0x02
#define EI_MAG3 0x03
#define EI_CLASS 0x04
#define EI_DATA 0x05
#define EI_VERSION 0x06
#define EI_OSABI 0x07
#define EI_ABIVERSION 0x08
#define EI_PAD 0x09

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ET_NONE 0x00
#define ET_REL 0x01
#define ET_EXEC 0x02
#define ET_DYN 0x03
#define ET_CORE 0x04
#define ET_LOOS 0xfe00
#define ET_HIOS 0xfeff
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

#define PT_NULL 0x00
#define PT_LOAD 0x01
#define PT_DYNAMIC 0x02
#define PT_INTERP 0x03
#define PT_NOTE 0x04
#define PT_SHLIB 0x05
#define PT_PHDR 0x06
#define PT_TLS 0x07
#define PT_LOOS 0x60000000
#define PT_HIOS 0x6FFFFFFF
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7FFFFFFF

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define SHT_NULL 0x00
#define SHT_PROGBITS 0x01
#define SHT_SYMTAB 0x02
#define SHT_STRTAB 0x03
#define SHT_RELA 0x04
#define SHT_HASH 0x05
#define SHT_DYNAMIC 0x06
#define SHT_NOTE 0x07
#define SHT_NOBITS 0x08
#define SHT_REL 0x09
#define SHT_SHLIB 0x0a
#define SHT_DYNSYM 0x0b
#define SHT_INIT_ARRAY 0x0e
#define SHT_FINI_ARRAY 0x0f
#define SHT_PREINIT_ARRAY 0x10
#define SHT_GROUP 0x11
#define SHT_SYMTAB_SHNDX 0x12
#define SHT_NUM 0x13
#define SHT_LOOS 0x60000000
#define SHT_HIOS 0x6FFFFFFF
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7FFFFFFF

#define SHF_WRITE 0x001
#define SHF_ALLOC 0x002
#define SHF_EXECINSTR 0x004
#define SHF_MERGE 0x010
#define SHF_STRINGS 0x020
#define SHF_INFO_LINK 0x040
#define SHF_LINK_ORDER 0x080
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP 0x200
#define SHF_TLS 0x400
#define SHF_MASKOS 0x0FF00000
#define SHF_MASKPROC 0xF0000000

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;

#define R_CEN_NORMAL 0 /* normal relocation, 16 bit address */
#define R_CEN_PCREL 1 /* program counter-relative relocation- 8 bit signed offset from next instruction (?) */

#define ELF32_R_SYM(info) ((info) >> 8)
#define ELF32_R_TYPE(info) ((uint8_t) info)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type) & 0xff))

typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
} Elf32_Rel;

typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t r_addend;
} Elf32_Rela;

#define SHN_UNDEF 0x0
#define SHN_LORESERVE 0xff00
#define SHN_LOPROC 0xff00
#define SHN_HIPROC 0xff1f
#define SHN_LOOS 0xff20
#define SHN_HIOS 0xff3f
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_XINDEX 0xffff
#define SHN_HIRESERVE 0xffff

#define ELF32_ST_BIND(info) ((info) >> 4)
#define ELF32_ST_TYPE(info) ((info) & 0xf)
#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define ELF32_ST_VISIBILITY(other) ((other) & 0x3)

#define STB_LOCAL 0x0
#define STB_GLOBAL 0x1
#define STB_WEAK 0x2
#define STB_LOOS 0xa
#define STB_HIOS 0xc
#define STB_LOPROC 0xd
#define STB_HIPROC 0xf

#define STT_NOTYPE 0x0
#define STT_OBJECT 0x1
#define STT_FUNC 0x2
#define STT_SECTION 0x3
#define STT_FILE 0x4
#define STT_COMMON 0x5
#define STT_TLS 0x6
#define STT_LOOS 0xa
#define STT_HIOS 0xc
#define STT_LOPROC 0xd
#define STT_HIPROC 0xf

#define STV_DEFAULT 0x0
#define STV_INTERNAL 0x1
#define STV_HIDDEN 0x2
#define STV_PROTECTED 0x3

typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
} Elf32_Sym;

#endif
