const std = @import("std");
const arch = @import("arch/i386.zig");
const process = @import("process.zig");

const SyscallErrors = error{SyscallError};

const debug_msg_scope = std.log.scoped(.debug_syscall);

pub const Syscalls = enum(usize) {
    isComputerOn,
    receive,
    reply,
    call,
    yield,
    _,
};

pub fn syscall(context: *arch.Context, num: usize, arguments: [3]usize) SyscallErrors!usize {
    _ = arguments;

    const syscall_kind: Syscalls = @enumFromInt(num);
    switch (syscall_kind) {
        .isComputerOn => return 1,
        .receive => {},
        .reply => {},
        .call => {},
        .yield => process.yield(context),
        _ => return SyscallErrors.SyscallError,
    }

    return 0;
}
