ENTRY(start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    . = 0x100000;
    . += SIZEOF_HEADERS;

    .init : AT(ADDR(.init)) {
        KEEP(*(.multiboot))
        *(.inittext)
    }

    kernel_start = .;

    .text : AT(ADDR(.text)) {
        *(.text .text.*)
    }

    .rodata : AT(ADDR(.rodata)) {
        *(.rodata .rodata.*)
    }

    .data : AT(ADDR(.data)) {
        *(.padata)
        *(.data .data.*)
    }

    /* zero-initialised data */
    .bss : AT(ADDR(.bss)) {
        *(.bss .bss.*)
    }

    kernel_end = .;
}
