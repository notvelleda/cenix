const std = @import("std");
const io = @import("io.zig");

// http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
const IDTEntry = packed struct {
    base_lo: u16,   // The lower 16 bits of the address to jump to when this interrupt fires.
    sel: u16,       // Kernel segment selector.
    always0: u8,    // This must always be zero.
    flags: u8,      // More flags. See documentation.
    base_hi: u16,   // The upper 16 bits of the address to jump to.
};

const IDTPtr = packed struct {
    limit: u16,
    base: u32,
};

const IntRegisters = extern struct {
    ds: u32,    // Data segment selector
    // Pushed by pusha.
    edi: u32,
    esi: u32,
    ebp: u32,
    handler_esp: u32,
    ebx: u32,
    edx: u32,
    ecx: u32,
    eax: u32,
    // Interrupt number and error code (if applicable)
    int_no: u32,
    error_code: u32,
    // Pushed by the processor automatically.
    eip: u32,
    cs: u32,
    eflags: u32,
    esp: u32,
    ss: u32,
};

const IDTEntries = 256;

// http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
fn makeIDTEntry(base: u32, sel: u16, flags: u8) IDTEntry {
    return .{
        .base_lo = @intCast(base & 0xffff),
        .base_hi = @intCast((base >> 16) & 0xffff),

        .sel = sel,
        .always0 = 0,
        .flags = flags,
    };
} 

var IDT: [IDTEntries]IDTEntry = undefined;

extern fn isr0() callconv(.C) void;
extern fn isr1() callconv(.C) void;
extern fn isr2() callconv(.C) void;
extern fn isr3() callconv(.C) void;
extern fn isr4() callconv(.C) void;
extern fn isr5() callconv(.C) void;
extern fn isr6() callconv(.C) void;
extern fn isr7() callconv(.C) void;
extern fn isr8() callconv(.C) void;
extern fn isr9() callconv(.C) void;
extern fn isr10() callconv(.C) void;
extern fn isr11() callconv(.C) void;
extern fn isr12() callconv(.C) void;
extern fn isr13() callconv(.C) void;
extern fn isr14() callconv(.C) void;
extern fn isr15() callconv(.C) void;
extern fn isr16() callconv(.C) void;
extern fn isr17() callconv(.C) void;
extern fn isr18() callconv(.C) void;
extern fn isr19() callconv(.C) void;
extern fn isr20() callconv(.C) void;
extern fn isr21() callconv(.C) void;
extern fn isr22() callconv(.C) void;
extern fn isr23() callconv(.C) void;
extern fn isr24() callconv(.C) void;
extern fn isr25() callconv(.C) void;
extern fn isr26() callconv(.C) void;
extern fn isr27() callconv(.C) void;
extern fn isr28() callconv(.C) void;
extern fn isr29() callconv(.C) void;
extern fn isr30() callconv(.C) void;
extern fn isr31() callconv(.C) void;
extern fn isr32() callconv(.C) void;
extern fn isr33() callconv(.C) void;
extern fn isr34() callconv(.C) void;
extern fn isr35() callconv(.C) void;
extern fn isr36() callconv(.C) void;
extern fn isr37() callconv(.C) void;
extern fn isr38() callconv(.C) void;
extern fn isr39() callconv(.C) void;
extern fn isr40() callconv(.C) void;
extern fn isr41() callconv(.C) void;
extern fn isr42() callconv(.C) void;
extern fn isr43() callconv(.C) void;
extern fn isr44() callconv(.C) void;
extern fn isr45() callconv(.C) void;
extern fn isr46() callconv(.C) void;
extern fn isr47() callconv(.C) void;

pub fn initIDT() void {
    @memset(&IDT, .{
        .base_lo = 0,
        .base_hi = 0,

        .sel = 0,
        .always0 = 0,
        .flags = 0,
    });

    IDT[0] = makeIDTEntry(@intFromPtr(&isr0), 0x08, 0x8e);
    IDT[1] = makeIDTEntry(@intFromPtr(&isr1), 0x08, 0x8e);
    IDT[2] = makeIDTEntry(@intFromPtr(&isr2), 0x08, 0x8e);
    IDT[3] = makeIDTEntry(@intFromPtr(&isr3), 0x08, 0x8e);
    IDT[4] = makeIDTEntry(@intFromPtr(&isr4), 0x08, 0x8e);
    IDT[5] = makeIDTEntry(@intFromPtr(&isr5), 0x08, 0x8e);
    IDT[6] = makeIDTEntry(@intFromPtr(&isr6), 0x08, 0x8e);
    IDT[7] = makeIDTEntry(@intFromPtr(&isr7), 0x08, 0x8e);
    IDT[8] = makeIDTEntry(@intFromPtr(&isr8), 0x08, 0x8e);
    IDT[9] = makeIDTEntry(@intFromPtr(&isr9), 0x08, 0x8e);
    IDT[10] = makeIDTEntry(@intFromPtr(&isr10), 0x08, 0x8e);
    IDT[11] = makeIDTEntry(@intFromPtr(&isr11), 0x08, 0x8e);
    IDT[12] = makeIDTEntry(@intFromPtr(&isr12), 0x08, 0x8e);
    IDT[13] = makeIDTEntry(@intFromPtr(&isr13), 0x08, 0x8e);
    IDT[14] = makeIDTEntry(@intFromPtr(&isr14), 0x08, 0x8e);
    IDT[15] = makeIDTEntry(@intFromPtr(&isr15), 0x08, 0x8e);
    IDT[16] = makeIDTEntry(@intFromPtr(&isr16), 0x08, 0x8e);
    IDT[17] = makeIDTEntry(@intFromPtr(&isr17), 0x08, 0x8e);
    IDT[18] = makeIDTEntry(@intFromPtr(&isr18), 0x08, 0x8e);
    IDT[19] = makeIDTEntry(@intFromPtr(&isr19), 0x08, 0x8e);
    IDT[20] = makeIDTEntry(@intFromPtr(&isr20), 0x08, 0x8e);
    IDT[21] = makeIDTEntry(@intFromPtr(&isr21), 0x08, 0x8e);
    IDT[22] = makeIDTEntry(@intFromPtr(&isr22), 0x08, 0x8e);
    IDT[23] = makeIDTEntry(@intFromPtr(&isr23), 0x08, 0x8e);
    IDT[24] = makeIDTEntry(@intFromPtr(&isr24), 0x08, 0x8e);
    IDT[25] = makeIDTEntry(@intFromPtr(&isr25), 0x08, 0x8e);
    IDT[26] = makeIDTEntry(@intFromPtr(&isr26), 0x08, 0x8e);
    IDT[27] = makeIDTEntry(@intFromPtr(&isr27), 0x08, 0x8e);
    IDT[28] = makeIDTEntry(@intFromPtr(&isr28), 0x08, 0x8e);
    IDT[29] = makeIDTEntry(@intFromPtr(&isr29), 0x08, 0x8e);
    IDT[30] = makeIDTEntry(@intFromPtr(&isr30), 0x08, 0x8e);
    IDT[31] = makeIDTEntry(@intFromPtr(&isr31), 0x08, 0x8e);
    IDT[32] = makeIDTEntry(@intFromPtr(&isr32), 0x08, 0x8e);
    IDT[33] = makeIDTEntry(@intFromPtr(&isr33), 0x08, 0x8e);
    IDT[34] = makeIDTEntry(@intFromPtr(&isr34), 0x08, 0x8e);
    IDT[35] = makeIDTEntry(@intFromPtr(&isr35), 0x08, 0x8e);
    IDT[36] = makeIDTEntry(@intFromPtr(&isr36), 0x08, 0x8e);
    IDT[37] = makeIDTEntry(@intFromPtr(&isr37), 0x08, 0x8e);
    IDT[38] = makeIDTEntry(@intFromPtr(&isr38), 0x08, 0x8e);
    IDT[39] = makeIDTEntry(@intFromPtr(&isr39), 0x08, 0x8e);
    IDT[40] = makeIDTEntry(@intFromPtr(&isr40), 0x08, 0x8e);
    IDT[41] = makeIDTEntry(@intFromPtr(&isr41), 0x08, 0x8e);
    IDT[42] = makeIDTEntry(@intFromPtr(&isr42), 0x08, 0x8e);
    IDT[43] = makeIDTEntry(@intFromPtr(&isr43), 0x08, 0x8e);
    IDT[44] = makeIDTEntry(@intFromPtr(&isr44), 0x08, 0x8e);
    IDT[45] = makeIDTEntry(@intFromPtr(&isr45), 0x08, 0x8e);
    IDT[46] = makeIDTEntry(@intFromPtr(&isr46), 0x08, 0x8e);
    IDT[47] = makeIDTEntry(@intFromPtr(&isr47), 0x08, 0x8e);

    var idt_ptr = IDTPtr {
        .limit = @sizeOf(IDTEntry) * IDTEntries - 1,
        .base = @intFromPtr(&IDT),
    };

    asm volatile ("lidt (%[idt])"
        :
        : [idt] "{eax}" (&idt_ptr)
    );
}

// https://wiki.osdev.org/Exceptions
const interrupt_names: [32][]const u8 = .{
    "division error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating point exception",
    "alignment check",
    "machine check",
    "SIMD floating point exception",
    "virtualization exception",
    "control protection exception",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection exception",
    "VMM communication exception",
    "security exception",
    "reserved",
};

fn getExceptionName(exception: u32) []const u8 {
    if (exception < 32)
        return interrupt_names[exception];

    return "unknown";
}

export fn isrHandler(registers: IntRegisters) callconv(.C) void {
    if (registers.int_no == 3) {
        std.log.info("breakpoint interrupt!", .{});
        return;
    }

    if (registers.int_no < 32) {
        std.log.err("fatal exception {x:0>8} ({s}) at {x:0>8}, error code {x:0>8}", .{registers.int_no, getExceptionName(registers.int_no), registers.eip, registers.error_code});
        std.log.err("eax = {x:0>8}, ebx = {x:0>8}, ecx = {x:0>8}, edx = {x:0>8}", .{registers.eax, registers.ebx, registers.ecx, registers.edx});
        std.log.err("esi = {x:0>8}, edi = {x:0>8}, ebp = {x:0>8}, esp = {x:0>8}", .{registers.esi, registers.edi, registers.ebp, registers.esp});
        std.log.err("eip = {x:0>8}, eflags = {x:0>8}", .{registers.eip, registers.eflags});
        std.log.err("cs = {x:0>4}, ds = {x:0>4}, ss = {x:0>4}", .{registers.cs & 0xffff, registers.ds & 0xffff, registers.ss & 0xffff});
        @panic("fatal exception in kernel mode");
    }
}
