MULTIBOOT_PAGE_ALIGN  =  (1<<0)
MULTIBOOT_MEMORY_INFO =  (1<<1)
MULTIBOOT_REQVIDMODE  =  (1<<2)

MULTIBOOT_HEADER_MAGIC = 0x1badb002
MULTIBOOT_HEADER_FLAGS = (MULTIBOOT_MEMORY_INFO)
MULTIBOOT_CHECKSUM     = -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

.section .multiboot, "a"
.globl header

header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long MULTIBOOT_CHECKSUM
    .long header
    /* a.out kludge (not used, the kernel is elf) */
    .long 0, 0, 0, 0    /* load_addr, load_end_addr, bss_end_addr, entry_addr */
    /* Video mode */
    .long 0, 0, 0, 0    /* type, width, height, depth */

.section .inittext, "ax"
.globl start

start:
    mov %eax, mboot_sig
    mov %ebx, mboot_ptr

    lgdt gdt_ptr

    ljmp $0x08, $start_high

.section .text
.globl start_high
.extern kmain

start_high:
    /* Prep segment registers */
    mov $0x10, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov $stack_end, %esp

    call kmain

    /* If kmain returns, loop forever */
halt:
    cli
    hlt
    jmp halt

.section .data
.globl mboot_sig
.globl mboot_ptr
.globl gdt
.globl gdt_ptr

mboot_sig:
    .long 0
mboot_ptr:
    .long 0
gdt_ptr:
    .word gdt_end - gdt - 1
    .long gdt
gdt:
    .long 0x00000000, 0x00000000    /* 00 NULL Entry */
    .long 0x0000ffff, 0x00cf9a00    /* 08 PL0 Code */
    .long 0x0000ffff, 0x00cf9200    /* 10 PL0 Data */
    .long 0x0000ffff, 0x00cffa00    /* 18 PL3 Code */
    .long 0x0000ffff, 0x00cff200    /* 20 PL3 Data */
    .long 0x00000068, 0x00008900    /* 28 TSS */
gdt_end:

.section .bss
.globl stack_base
.globl stack_end

stack_base:
    .space 0x1000 * 8
stack_end:
