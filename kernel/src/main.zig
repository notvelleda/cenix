const std = @import("std");
const io = @import("arch/i386/io.zig");
const interrupts = @import("arch/i386/interrupts.zig");

export fn kmain() callconv(.C) void {
    std.log.info("HellOwOrld", .{});
    interrupts.initIDT();
    asm volatile ("int3");
    @panic(":3c");
}

pub const std_options = struct {
    pub const log_level = .info;
    pub const logFn = serialLogFn;
};

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

pub const SerialWriter = std.io.Writer(Context, anyerror, serialWrite);

pub fn serialLogFn(comptime level: std.log.Level, comptime scope: @TypeOf(.EnumLiteral), comptime format: []const u8, args: anytype) void {
    SerialWriter.print(SerialWriter, comptime level.asText() ++ " [" ++ @tagName(scope) ++ "] " ++ format ++ "\n", args) catch return;
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

export fn memset(ptr: [*c]u8, value: c_int, num: usize) [*c]u8 {
    var actual_ptr = ptr;
    for (0..num) |_| {
        actual_ptr.* = @intCast(value);
        actual_ptr += 1;
    }
    return ptr;
}
