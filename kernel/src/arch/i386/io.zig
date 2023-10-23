pub inline fn outb(addr: u16, data: u8) void {
    asm volatile ("outb %[data], %[addr]"
        :
        : [addr] "{dx}" (addr), [data] "{al}" (data)
        : "dx", "al"
    );
}

pub inline fn outl(addr: u16, data: u32) void {
    asm volatile ("outl %[data], %[addr]"
        :
        : [addr] "{dx}" (addr), [data] "{eax}" (data)
        : "dx", "al"
    );
}

pub inline fn inb(addr: u16) u8 {
    return asm volatile ("inb %[addr], %[ret]"
        : [ret] "={al}" (-> u8)
        : [addr] "{dx}" (addr)
        : "dx", "al"
    );
}
