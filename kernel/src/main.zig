const std = @import("std");

export fn kmain() callconv(.C) void {
    puts("HellOwOrld\n");
    @panic(":3c");
}

inline fn outb(addr: u16, data: u8) void {
    asm volatile ("outb %[data], %[addr]"
        :
        : [addr] "{dx}" (addr), [data] "{al}" (data)
        : "dx", "al"
    );
}

inline fn outl(addr: u16, data: u32) void {
    asm volatile ("outl %[data], %[addr]"
        :
        : [addr] "{dx}" (addr), [data] "{eax}" (data)
        : "dx", "al"
    );
}

inline fn inb(addr: u16) u8 {
    return asm volatile ("inb %[addr], %[ret]"
        : [ret] "={al}" (-> u8)
        : [addr] "{dx}" (addr)
        : "dx", "al"
    );
}

fn exit(code: u32) noreturn {
    @setCold(true);
    outl(0x501, code);
    while (true)
        asm volatile ("cli; hlt");
}

fn putc(c: u8) void {
    while ((inb(0x3f8 + 5) & 0x20) == 0) {}
    outb(0x3f8, c);
    outb(0xe9, c);
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
