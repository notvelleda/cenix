const std = @import("std");
const io = @import("i386/io.zig");
const interrupts = @import("i386/interrupts.zig");

const Context = struct {};

fn serialWrite(context: Context, bytes: []const u8) !usize {
    _ = context;

    for (bytes) |c| {
        while ((io.inb(0x3f8 + 5) & 0x20) == 0) {}
        io.outb(0x3f8, c);
        io.outb(0xe9, c);
    }
    return bytes.len;
}

const SerialWriter = std.io.Writer(Context, anyerror, serialWrite);

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
    interrupts.initIDT();
}
