const std = @import("std");
const io = @import("arch/i386/io.zig");
const interrupts = @import("arch/i386/interrupts.zig");

export fn kmain() callconv(.C) void {
    puts("HellOwOrld\n");
    interrupts.initIDT();
    asm volatile ("int3");
    @panic(":3c");
}

fn exit(code: u32) noreturn {
    @setCold(true);
    io.outl(0x501, code);
    while (true)
        asm volatile ("cli; hlt");
}

fn putc(c: u8) void {
    while ((io.inb(0x3f8 + 5) & 0x20) == 0) {}
    io.outb(0x3f8, c);
    io.outb(0xe9, c);
}

fn puts(s: []const u8) void {
    for (s) |c|
        putc(c);
}

export fn memset(ptr: [*c]u8, value: c_int, num: usize) [*c]u8 {
    var actual_ptr = ptr;
    for (0..num) |_| {
        actual_ptr.* = @intCast(value);
        actual_ptr += 1;
    }
    return ptr;
}

pub fn panic(message: []const u8, stack_trace: ?*std.builtin.StackTrace, addr: ?usize) noreturn {
    @setCold(true);
    _ = stack_trace;
    _ = addr;

    puts("PANIC: ");
    puts(message);
    putc('\n');
    exit(0x31);
}
