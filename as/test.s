#define MUX0_STATCTL 0xf200
#define MUX0_RXTX 0xf201

    .section .text
    .align 2

    //ld %al, $0x71
    //st %al, 0x100
    //ld %aw, $jsys_handler
    //st %aw, 0x101

    ld %aw, $timer_handler
    st %aw, 0x00ae // load interrupt handler into program counter of context 10
    ld %aw, $other_loop
    st %aw, 0x001e // set up entry point for 2nd context
    ld %aw, $0xdfff
    st %aw, 0x001a // stack pointer
    ld %aw, $oops
    st %aw, 0x00fe
    ld %aw, $0x0000
    st %aw, 0x00a8 // clear zw of context 10 (fastest timer frequency)
    st %aw, 0x00a6 // clear yw of context 10
    ld %aw, $0x9002
    st %aw, 0x001c // ctx 1 context register

    ld %aw, $0xefff
    st %aw, 0x009a // ctx 9 stack pointer
    ld %aw, $0x0000
    st %aw, 0x009c // ctx 9 context register
    ld %aw, $test_handler
    st %aw, 0x009e // ctx 9 program counter

    ld %al, $0xff
    st %al, test
    wpf1 $2, test
    rpf1 $16, test
    wpf1 $18, test
    //ld %al, $0xff
    //st %al, test
    rpf1 $240, test
    wpf1 $242, test
    rpf1 $248, test
    wpf1 $250, test

    //rpf $248, test
    //jsr print_page_table

    //rpf $249, test
    //jsr print_page_table

    //rpf $250, test
    //jsr print_page_table

    //rpf $248, test
    //wpf $250, test

    di
    ei
loop:
    ld %bw, $message1
    jsr write_string
    jmp loop

other_loop:
    //ld %bw, $message2
    //jsr write_string

    ri // syscall :3

    ld %bl, $225
    st %bl, MUX0_RXTX

    jmp other_loop

oops:
    ld %bw, $oops_message
    jsr write_string
oops_hlt:
    di
    hlt
    jmp oops_hlt

test_handler:
    ld %bw, $message3
    jsr write_string

    ld %aw, $0x1000 // TODO: need to detect which ctx this syscall came from to properly return
    st %aw, 0x009c
    ri
    jmp test_handler

timer_handler:
    // check so we don't context switch if we're in a syscall handler
    rlc
    ld %aw, %cw
    srr %aw, 12
    ld %bw, $9
    sub %aw, %bw
    bz _int_thdl_ret

    ld %aw, $0x0fff
    and %aw, %cw // mask out previous interrupt level

    ivr %yw
    bz _int_thdl0

    ld %aw, $0x0000
    jmp _int_thdl1
_int_thdl0:
    ld %aw, $0x1000
_int_thdl1:

    //rlc
    //slr %aw, 12
    ori %aw, %cw // load new interrupt level

_int_thdl_ret:
    clr %zw // clear zw to keep the timer going as fast as possible
    ri
    jmp timer_handler // workaround for lack of RIM (which doesn't save program counter)

print_page_table:
    ld %aw, $32
    ld %zw, %aw
    ld %aw, $test
    ld %yw, %aw

loop2:
    ld %al, (%yw ++)
    jsr write_num_byte
    ld %bl, $172
    jsr write_char

    dcr %zw
    bnz loop2
    ld %bl, $0x8d
    jsr write_char
    ld %bl, $0x8a
    jsr write_char
    rsr

// writes the string at the address provided in bw to the terminal
// al and bw aren't preserved
write_string:
    // wait for the terminal to be ready
    ld %al, MUX0_STATCTL
    srr %al, 2
    bnl write_string

    ld %al, (%bw ++)
    bz end_write_string
    st %al, MUX0_RXTX
    jmp write_string

end_write_string:
    rsr

// writes the character stored in bl to the terminal
// al isn't preserved
write_char:
    // wait for the terminal to be ready
    ld %al, MUX0_STATCTL
    srr %al, 2
    bnl write_char

    st %bl, MUX0_RXTX
    rsr

// writes the contents of al to the terminal as hexadecimal
// aw and bl aren't preserved
write_num_byte:
    slr %aw, 8
    jsr _int_wrnum
    jsr _int_wrnum
    rsr

// writes the contents of aw to the terminal as hexadecimal
// aw and bl aren't preserved
write_num:
    jsr _int_wrnum
    jsr _int_wrnum
    jsr _int_wrnum
    // fall thru
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

    .section .rodata
    .align 2

message1:
    .hstring "hello from ctx 1"
    .byte 0x8d, 0x8a, 0

message2:
    .hstring "hello from ctx 2"
    .byte 0x8d, 0x8a, 0

oops_message:
    .hstring "oops"
    .byte 0x8d, 0x8a, 0

message3:
    .hstring ":3c"
    .byte 0x8d, 0x8a, 0

test:
    .zero 32

digits:
    .hstring "0123456789abcdef"
