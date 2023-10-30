const std = @import("std");
const io = @import("i386/io.zig");
const interrupts = @import("i386/interrupts.zig");
const mm = @import("../mm.zig");

const log_scope = std.log.scoped(.arch);

extern const kernel_start: usize;
extern const kernel_end: usize;

pub const Context = interrupts.IntRegisters;

const WriteContext = struct {};

fn serialWrite(context: WriteContext, bytes: []const u8) !usize {
    _ = context;

    for (bytes) |c| {
        while ((io.inb(0x3f8 + 5) & 0x20) == 0) {}
        io.outb(0x3f8, c);
        io.outb(0xe9, c);
    }
    return bytes.len;
}

const SerialWriter = std.io.Writer(WriteContext, anyerror, serialWrite);

pub fn logFn(comptime level: std.log.Level, comptime scope: @TypeOf(.EnumLiteral), comptime format: []const u8, args: anytype) void {
    SerialWriter.print(SerialWriter, "{s: >5} [{s}] ", .{ level.asText(), @tagName(scope) }) catch return;
    SerialWriter.print(SerialWriter, format ++ "\n", args) catch return;
}

fn exit(code: u32) noreturn {
    @setCold(true);
    io.outl(0x501, code);
    while (true) {
        asm volatile ("cli; hlt");
    }
}

pub fn panic(message: []const u8, stack_trace: ?*std.builtin.StackTrace, addr: ?usize) noreturn {
    @setCold(true);
    _ = stack_trace;
    _ = addr;

    SerialWriter.print(SerialWriter, "PANIC: {s}\n", .{message}) catch exit(0x31);
    exit(0x31);
}

const GDTEntry = packed struct {
    lower: u32,
    upper: u32,
};

extern var gdt: [6]GDTEntry;

const TSS = packed struct {
    link: u16,
    reserved0: u16,
    esp0: u32,
    ss0: u16,
    reserved1: u16,
    esp1: u32,
    ss1: u16,
    reserved2: u16,
    esp2: u32,
    ss2: u16,
    reserved3: u16,
    cr3: u32,
    eip: u32,
    eflags: u32,
    eax: u32,
    ecx: u32,
    edx: u32,
    ebx: u32,
    esp: u32,
    ebp: u32,
    esi: u32,
    edi: u32,
    es: u16,
    reserved4: u16,
    cs: u16,
    reserved5: u16,
    ss: u16,
    reserved6: u16,
    ds: u16,
    reserved7: u16,
    fs: u16,
    reserved8: u16,
    gs: u16,
    reserved9: u16,
    ldtr: u16,
    reserved10: u32,
    iobp_offset: u16,
};

extern const gdt_ptr: u8; // it's not actually a u8 but it's not being modified here so whatever
extern const stack_end: u8;

pub fn init() void {
    mm.the_heap.init(.{
        .kernel_start = &kernel_start,
        .kernel_end = &kernel_end,
        .memory_start = @ptrFromInt(4),
        .memory_end = @ptrFromInt(640 * 1024),
    });

    const tss: *TSS = @alignCast(@ptrCast(mm.the_heap.alloc(@sizeOf(TSS)) catch @panic("allocation failed")));

    tss.iobp_offset = @sizeOf(TSS);
    tss.ss0 = 0x10;
    tss.ss1 = tss.ss0;
    tss.ss2 = tss.ss0;
    tss.esp0 = @intFromPtr(&stack_end);
    tss.esp1 = tss.esp0;
    tss.esp2 = tss.esp0;

    log_scope.debug("tss @ {x:0>8}, int stack top @ {x:0>8}", .{ @intFromPtr(tss), tss.esp0 });

    const tss_addr = @intFromPtr(tss);
    gdt[5].lower |= (tss_addr & 0xffff) << 16;
    gdt[5].upper |= ((tss_addr >> 16) & 0xff) | (tss_addr & 0xff000000);

    // reload GDT
    asm volatile ("lgdt (%[gdt])"
        :
        : [gdt] "{eax}" (&gdt_ptr),
    );

    // load TSS
    asm volatile ("mov $0x28, %ax; ltr %ax" ::: "ax");

    interrupts.initIDT();
}
