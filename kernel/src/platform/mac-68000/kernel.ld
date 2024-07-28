ENTRY(_start)
OUTPUT_FORMAT(elf32-m68k)

MEMORY {
    ram : ORIGIN = 0x2000, LENGTH = 0x400000 - 0x2000
}

SECTIONS {
    .text : {
        *(.text)
        . = ALIGN(4);
    } > ram

    .data : {
        *(.rodata)
        . = ALIGN(4);
        *(.data)
        . = ALIGN(4);
    } > ram

    .bss : {
        *(.bss)
        . = ALIGN(4);
        *(COMMON)
        . = ALIGN(4);
        _end = .;
    } > ram
}
