const std = @import("std");
const arch = @import("arch/i386.zig");
const process = @import("process.zig");

const SyscallErrors = error{SyscallError};

const debug_msg_scope = std.log.scoped(.debug_syscall);

pub const Syscalls = enum(usize) {
    isComputerOn,
    yield,
    debugMessage,
    _,
};

pub fn syscall(context: *arch.Context, num: usize, arguments: [3]usize) SyscallErrors!usize {
    const syscall_kind: Syscalls = @enumFromInt(num);
    switch (syscall_kind) {
        .isComputerOn => return 1,
        .yield => process.yield(context),
        .debugMessage => {
            const addr: [*]u8 = @ptrFromInt(arguments[0]);
            const len: usize = @intCast(arguments[1]);
            debug_msg_scope.info("{s}", .{addr[0..len]});
        },
        _ => return SyscallErrors.SyscallError,
    }

    return 0;
}
