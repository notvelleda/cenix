#define MMIO_BASE 0xf000

#define MUX0_STATCTL MMIO_BASE + 0x200
#define MUX0_RXTX MMIO_BASE + 0x201

#define CTX0_BASE 0x0000
#define CTX1_BASE 0x0010
#define CTX2_BASE 0x0020
#define CTX3_BASE 0x0030
#define CTX4_BASE 0x0040
#define CTX5_BASE 0x0050
#define CTX6_BASE 0x0060
#define CTX7_BASE 0x0070
#define CTX8_BASE 0x0080
#define CTX9_BASE 0x0090
#define CTX10_BASE 0x00a0
#define CTX11_BASE 0x00b0
#define CTX12_BASE 0x00c0
#define CTX13_BASE 0x00d0
#define CTX14_BASE 0x00e0
#define CTX15_BASE 0x00f0

#define REG_AW_NIBBLE 0x0
#define REG_AH_NIBBLE 0x0
#define REG_AL_NIBBLE 0x1
#define REG_BW_NIBBLE 0x2
#define REG_BH_NIBBLE 0x2
#define REG_BL_NIBBLE 0x3
#define REG_XW_NIBBLE 0x4
#define REG_XH_NIBBLE 0x4
#define REG_XL_NIBBLE 0x5
#define REG_YW_NIBBLE 0x6
#define REG_YH_NIBBLE 0x6
#define REG_YL_NIBBLE 0x7
#define REG_ZW_NIBBLE 0x8
#define REG_ZH_NIBBLE 0x8
#define REG_ZL_NIBBLE 0x9
#define REG_SW_NIBBLE 0xa
#define REG_SH_NIBBLE 0xa
#define REG_SL_NIBBLE 0xb
#define REG_CW_NIBBLE 0xc
#define REG_CH_NIBBLE 0xc
#define REG_CL_NIBBLE 0xd
#define REG_PW_NIBBLE 0xe
#define REG_PH_NIBBLE 0xe
#define REG_PL_NIBBLE 0xf

#define NUM_ABORT_MSGS 6

#define NUM_PAGES 128
#define PAGE_SIZE 2048

#define KERN_STACK_SIZE 0x800
#define KERN_STACK_END MMIO_BASE
#define KERN_STACK_START KERN_STACK_END - KERN_STACK_SIZE

#define HEAP_START 0x4000
#define HEAP_END HEAP_START + PAGE_SIZE * 2
#define HEAP_MAX KERN_STACK_START - PAGE_SIZE /* gotta leave a page in between
                                               * to catch stack overflow
                                               */

    .section .text
    .align 2

kernel_start:
    ld %aw, $KERN_STACK_END
    st %aw, CTX15_BASE + REG_SW_NIBBLE
    xfr %aw, %sw
    ld %aw, $abort_handler
    st %aw, CTX15_BASE + REG_PW_NIBBLE

    ld %aw, $timer_handler
    st %aw, CTX10_BASE + REG_PW_NIBBLE
    ld %aw, $0
    st %aw, CTX10_BASE + REG_ZW_NIBBLE

    ld %bw, $newline_str
    jsr write_string

#if 0
    /*ld %al, $131*/
    ld %al, $0xfd
    push %al
    ld %al, $0
    ld %bl, $3
    slr %bl, 3
    ori %al, %bl
    push %bl
    lsm (%sw), 1(%sw)
    inr %sw /* no need to pop anything */

    ld %al, $0
    ld %bl, $3
    slr %bl, 3
    ori %al, %bl
    push %bl
    ssm (%sw), (%sw)
    pop %al
    jsr write_num_byte

    ld %aw, 6144
    /*st %aw, 6144*/
#endif

    /* mark reserved pages as used */
    ld %al, $0 /* 1st page contains cpu registers */
    jsr mark_page_used
    ld %al, $0x7e /* mmio page 1 */
    jsr mark_page_used
    ld %al, $0x7f /* mmio page 2 */
    jsr mark_page_used

    ld %bw, $kernel_start
    ld %aw, $kernel_end
    jsr set_range_used

    ld %bw, $KERN_STACK_START
    ld %aw, $KERN_STACK_END
    jsr set_range_used

    /* partial memory test to try and get a decent memory map */
    jsr partial_ram_test

    /* print out how much ram was detected */
    jsr get_free
    clr %au
    slr %aw
    jsr write_num_dec_unsigned
    ld %bw, $detected_memory_msg
    jsr write_string

    /* pages 0x70-0x7d don't seem to be accessible for some reason?? */
#if 0
    /* workaround for bit 1 of page table entries not being checked when the
     * cpu is checking whether to trap both reads and writes, causing 0xff and
     * 0xfd to be equivalent: simply just use the page in a place where it's
     * extremely unlikely to be freed (in this case the start of the heap),
     * and add an extra reference to the page just in case it is freed somehow
     */
    ld %aw, $HEAP_START >> 11

    slr %al, 3
    push %al
    ld %al, $0x7d
    push %al
    lsm 1(%sw), (%sw)

    jsr mark_page_used
    pop %al
    jsr mark_page_used
    inr %sw
#endif

#if 0
    jsr alloc_page
    jsr write_num_byte /* 0x01 */
    jsr alloc_page
    jsr write_num_byte /* 0x03 */
    jsr alloc_page
    jsr write_num_byte /* 0x04 */
    ld %al, $3
    jsr remove_page_ref /* free page 3 */
    jsr alloc_page
    jsr write_num_byte /* 0x03*/
#endif

    /* remove unused pages from kernel page table */
    ld %bw, $0x100
    ld %aw, $kernel_start
    dcr %aw
    jsr free_range

    ld %bw, $kernel_end
    ld %aw, $KERN_STACK_START
    dcr %aw
    jsr free_range

    /* allocate memory for and initialize heap */
    ld %aw, $kernel_heap
    xfr %aw, %yw
    ld %aw, $HEAP_END
    jsr expand_heap
    jsr init_heap

    ld %aw, $0x6000
    jsr expand_heap
    ld %aw, 2(%yw)
    jsr write_num
    ld %aw, $0xd000
    jsr contract_heap
    ld %aw, 2(%yw)
    jsr write_num

#if 0
    ld %aw, $12
    jsr find_smallest_hole
    jsr write_num

    ld %aw, 2(%yw)
    jsr write_num
#endif

    ei

loop:
    /*.byte 0x87*/
    hlt
    jmp loop

timer_handler:
    ri
    jmp timer_handler

/* ctx 15 (abort trap) handler */
abort_handler:
    push .. /* push all registers */

    /*ld %aw, has_kernel_init */
    /* ... */

    /* write panic msg with context number */
    ld %bw, $cpu_abort_msg
    jsr write_string
    rl
    xfr %cw, %aw
    srr %aw, 12
    jsr write_num_dec_unsigned
    ld %bw, $after_fault_msg
    jsr write_string

    /* check if there's a human readable message for this abort code */
    ld %aw, (%sw)
    ld %bw, $NUM_ABORT_MSGS
    sub %aw, %bw
    bm _int_phdl_amsg

    /* if there isn't, print a message to show that and the abort code */
    ld %bw, $unknown_abort_msg
    jsr write_string
    ld %aw, (%sw)
    jsr write_num_byte
    jmp _int_phdl_nl

_int_phdl_amsg:
    /* if there is, print the human readable message for this abort code */
    ld %aw, (%sw)
    rl
    slr %aw
    ld %bw, $abort_trap_tbl
    add %bw, %aw
    ld %bw, (%aw)
    jsr write_string

_int_phdl_nl:
    /* print newlines after the panic message */
    ld %bw, $newline_str
    jsr write_string
    ld %bw, $newline_str
    jsr write_string

    /* dump all registers */
    ld %aw, $7
    xfr %aw, %zw

    jsr dump_regs

    /* dump 2nd to last ctx as normal */
    jsr dump_ctx

    ld %bl, $160
    jsr write_char
    jsr write_char

    xfr %sw, %yw
    jsr dump_ctx

    ld %bw, $reg_hdr_ftr_msg
    jsr write_string

panic_hlt:
    di
    hlt
    jmp panic_hlt

/* manually triggers a kernel panic without an abort trap */
manual_panic:
    di
    push %aw

    ld %bw, $panic_msg
    jsr write_string
    pop %bw
    jsr write_string

    ld %bw, $newline_str
    jsr write_string
    ld %bw, $newline_str
    jsr write_string

    jsr dump_all_regs

    /* switch to ctx 15 and halt */
    ld %aw, $panic_hlt
    st %aw, CTX15_BASE + REG_PW_NIBBLE /* load pc for ctx 15 */

    ld %bw, $0xf000
    ori %bw, %cw /* set previous ctx to 15 */

    ri /* ctx switch */

dump_all_regs:
    ld %aw, $8
    xfr %aw, %zw
    jsr dump_regs

    ld %bw, $reg_hdr_ftr_msg
    jsr write_string
    rsr

/* dumps registers for all contexts
 * xw is the only preserved register
 */
dump_regs:
    ld %bw, $reg_dump_msg
    jsr write_string

    ld %bw, $reg_hdr_ftr_msg
    jsr write_string

    ld %aw, $CTX0_BASE
    xfr %aw, %yw

_int_dreg_loop:
    push %zw

    jsr dump_ctx

    ld %bl, $160
    jsr write_char
    jsr write_char

    jsr dump_ctx

    pop %zw

    dcr %zw
    bnz _int_dreg_loop

    rsr

/* dumps registers for a single context
 * all registers except bh maybe? and xw aren't preserved
 */
dump_ctx:
    ld %aw, $8
    xfr %aw, %zw

_int_dctx_loop:
    ld %aw, (%yw)
    jsr write_num
    inr %yw, 2

    dcr %zw
    bz _int_dctx_finish

    ld %bl, $160
    jsr write_char

    jmp _int_dctx_loop

_int_dctx_finish:
    rsr

/* writes the string at the address provided in bw to the terminal
 * al and bw aren't preserved
 */
write_string:
    /* wait for the terminal to be ready */
    ld %al, MUX0_STATCTL
    srr %al, 2
    bnl write_string

    ld %al, (%bw ++)
    bz _int_wstr_end
    st %al, MUX0_RXTX
    jmp write_string

_int_wstr_end:
    rsr

/* writes the character stored in bl to the terminal
 * al isn't preserved
 */
write_char:
    /* wait for the terminal to be ready */
    ld %al, MUX0_STATCTL
    srr %al, 2
    bnl write_char

    st %bl, MUX0_RXTX
    rsr

/* writes the contents of al to the terminal as hexadecimal
 * aw and bl aren't preserved
 */
write_num_byte:
    slr %aw, 8
    jsr _int_wrnum
    jsr _int_wrnum
    rsr

/* writes the contents of aw to the terminal as hexadecimal
 * aw and bl aren't preserved
 */
write_num:
    jsr _int_wrnum
    jsr _int_wrnum
    jsr _int_wrnum
    /* fall thru */
_int_wrnum:
    push %aw

    srr %aw, 12
    ld %bw, $0x000f
    and %aw, %bw

    ld %aw, $digits
    add %aw, %bw
    ld %bl, (%bw)
    jsr write_char

    pop %aw

    slr %aw, 4

    rsr

/* writes a signed number in aw to the terminal as base 10
 * aw and bw aren't preserved
 */
write_num_dec:
    /* check if this number is negative */
    xfr %aw, %bw
    slr %bw
    bnl write_num_dec_unsigned

    /* it is, make it positive and print a negative sign */
    ivr %aw
    inr %aw

    xfr %al, %bu
    ld %bl, $0xad
    jsr write_char
    xfr %bu, %al

/* writes an unsigned number in aw to the terminal as base 10
 * aw and bw aren't preserved
 */
write_num_dec_unsigned:
    ld %bw, $10
    sub %aw, %bw
    bm _int_wnd_out

    ld %bw, $10
    divu %bw, %aw
    push %aw

    xfr %bw, %aw
    jsr write_num_dec_unsigned

    pop %aw

_int_wnd_out:
    /* print this digit */
    ld %bl, $0xb0
    add %al, %bl

    xfr %al, %bu
    jsr write_char
    xfr %bu, %al

    rsr

#include "vmm.s"
#include "malloc.s"

    .section .rodata
    .align 2

panic_msg:
    .byte 0x8d, 0x8a
    .hstring "PANIC: "
    .byte 0

oom_msg:
    .hstring "out of memory"
    .byte 0

excess_shmem_msg:
    .hstring "too much shared memory"
    .byte 0

bad_heap_msg:
    .hstring "kernel heap state invalid"
    .byte 0

cpu_abort_msg:
    .byte 0x8d, 0x8a
    .hstring "PANIC: abort trap in context "
    .byte 0

after_fault_msg:
    .hstring ": "
    .byte 0

/* allows the abort trap handler to easily transform condition numbers into
 * human readable messages
 */
abort_trap_tbl:
    .short ill_inst_msg, attempted_hlt_msg, ill_write_msg, ill_read_msg
    .short parity_err_msg, math_overflow_msg

ill_inst_msg:
    .hstring "illegal instruction"
    .byte 0

attempted_hlt_msg:
    .hstring "attempted HLT"
    .byte 0

ill_write_msg:
    .hstring "illegal memory write"
    .byte 0

ill_read_msg:
    .hstring "illegal memory read"
    .byte 0

parity_err_msg:
    .hstring "parity error"
    .byte 0

math_overflow_msg:
    .hstring "math overflow"
    .byte 0

unknown_abort_msg:
    .hstring "unknown abort code 0x"
    .byte 0

newline_str:
    .byte 0x8d, 0x8a, 0

reg_dump_msg:
    .hstring "full register dump, contexts 0-15 in order "
    .hstring "(left to right, downwards):"
    .byte 0x8d, 0x8a, 0

reg_hdr_ftr_msg:
    .hstring "A----B----X----Y----Z----S----C----P---||"
    .hstring "A----B----X----Y----Z----S----C----P---"
    .byte 0

detected_memory_msg:
    .hstring " KiB of usable memory detected"
    .byte 0x8d, 0x8a, 0

digits:
    .hstring "0123456789abcdef"
    .byte 0

    .section .data
    .align 2

has_kernel_init:
    .byte 0

/* list of how many references each memory page has */
kernel_heap:
    .short HEAP_START, HEAP_START, HEAP_END, HEAP_MAX, HEAP_START

vmm_pages:
    .zero NUM_PAGES

kernel_end:

    .section .end
    .align 2048
page_after:
