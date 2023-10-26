const std = @import("std");
const arch = @import("arch/i386.zig");
const mm = @import("mm.zig");

pub const std_options = struct {
    pub const log_level = .debug;
    pub const logFn = arch.logFn;
};
pub const panic = arch.panic;

export fn kmain() callconv(.C) void {
    arch.init();
    std.log.info("HellOwOrld", .{});
    mm.listBlocks();
    var ptr1 = mm.alloc(1024) catch @panic("allocation failed");
    mm.listBlocks();
    var ptr2 = mm.alloc(1024) catch @panic("allocation failed");
    mm.listBlocks();
    var ptr3 = mm.alloc(1024) catch @panic("allocation failed");
    mm.listBlocks();
    mm.free(ptr2);
    std.log.debug("freed memory", .{});
    mm.listBlocks();
    var ptr4 = mm.alloc(1024) catch @panic("allocation failed");
    mm.listBlocks();
    mm.free(ptr1);
    std.log.debug("freed memory", .{});
    mm.listBlocks();
    mm.free(ptr4);
    std.log.debug("freed memory", .{});
    mm.listBlocks();
    mm.free(ptr3);
    std.log.debug("freed memory", .{});
    mm.listBlocks();
    asm volatile ("int3");
    @panic(":3c");
}

export fn memset(ptr: [*c]u8, value: c_int, num: usize) [*c]u8 {
    var actual_ptr = ptr;
    for (0..num) |_| {
        actual_ptr.* = @intCast(value);
        actual_ptr += 1;
    }
    return ptr;
}

export fn memcpy(dest: [*c]u8, src: [*c]u8, num: usize) [*c]u8 {
    var actual_dest = dest;
    var actual_src = src;
    for (0..num) |_| {
        actual_dest.* = actual_src.*;
        actual_dest += 1;
        actual_src += 1;
    }
    return dest;
}
