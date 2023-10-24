const std = @import("std");
const arch = @import("arch/i386.zig");

pub const std_options = struct {
    pub const log_level = .info;
    pub const logFn = arch.logFn;
};
pub const panic = arch.panic;

export fn kmain() callconv(.C) void {
    arch.init();
    std.log.info("HellOwOrld", .{});
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
