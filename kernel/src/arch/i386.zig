const std = @import("std");
const io = @import("i386/io.zig");
const interrupts = @import("i386/interrupts.zig");
const mm = @import("../mm.zig");

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
    SerialWriter.print(SerialWriter, "{s: >5} [{s}] ", .{level.asText(), @tagName(scope)}) catch return;
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

pub fn init() void {
    mm.init(.{
        .kernel_start = &kernel_start,
        .kernel_end = &kernel_end,
        .memory_start = @ptrFromInt(4),
        .memory_end = @ptrFromInt(640 * 1024),
    });
    interrupts.initIDT();
}
