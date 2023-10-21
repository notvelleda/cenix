const builtin = @import("builtin");

export fn kmain() callconv(.C) void {
    puts("HellOwOrld\n");
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
    outl(0x501, code);
    while (true)
        asm volatile ("cli; hlt");
}

fn putchar(c: u8) void {
    while ((inb(0x3f8 + 5) & 0x20) == 0) {}
    outb(0x3f8, c);
    outb(0xe9, c);
}

fn puts(s: []const u8) void {
    for (s) |c|
        putchar(c);
}

export fn memset(ptr: [*]u8, value: c_int, num: usize) [*]u8 {
    var i: usize = 0;
    while (i < num) : (i += 1)
        ptr[i] = @intCast(u8, value);
    return ptr;
}
