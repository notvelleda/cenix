/* http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html */

.macro isrNoErrCode num
.globl isr\num
isr\num:
    cli
    push $0
    push $\num
    jmp isrCommonStub
    iret
.endm

.macro isrErrCode num
.globl isr\num
isr\num:
    cli
    push $\num
    jmp isrCommonStub
.endm

isrNoErrCode 0
isrNoErrCode 1
isrNoErrCode 2
isrNoErrCode 3
isrNoErrCode 4
isrNoErrCode 5
isrNoErrCode 6
isrNoErrCode 7
isrErrCode 8
isrNoErrCode 9
isrErrCode 10
isrErrCode 11
isrErrCode 12
isrErrCode 13
isrErrCode 14
isrNoErrCode 15
isrNoErrCode 16
isrErrCode 17
isrNoErrCode 18
isrNoErrCode 19
isrNoErrCode 20
isrErrCode 21
isrNoErrCode 22
isrNoErrCode 23
isrNoErrCode 24
isrNoErrCode 25
isrNoErrCode 26
isrNoErrCode 27
isrNoErrCode 28
isrErrCode 29
isrErrCode 30
isrNoErrCode 31
isrNoErrCode 32
isrNoErrCode 33
isrNoErrCode 34
isrNoErrCode 35
isrNoErrCode 36
isrNoErrCode 37
isrNoErrCode 38
isrNoErrCode 39
isrNoErrCode 40
isrNoErrCode 41
isrNoErrCode 42
isrNoErrCode 43
isrNoErrCode 44
isrNoErrCode 45
isrNoErrCode 46
isrNoErrCode 47
isrNoErrCode 128

.extern isrHandler

isrCommonStub:
    pusha

    mov %ds, %ax
    push %eax

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %esp, %ebp
    push %esp
    call isrHandler
    add $4, %esp

    pop %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    popa
    add $8, %esp
    sti
    iret
