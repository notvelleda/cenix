const std = @import("std");
const arch = @import("arch/i386.zig");

const log_scope = std.log.scoped(.process);

pub const Thread = struct {
    context: arch.Context,

    next: ?*Thread,
};

var current_thread: ?*Thread = null;

pub fn yield(context: *arch.Context) void {
    if (current_thread) |thread| {
        thread.context = context.*;
        current_thread = thread.next;

        if (current_thread) |new_thread| {
            context.* = new_thread.context;
        }
    } else {
        log_scope.debug("no current thread", .{});
    }
}

pub fn setCurrentThread(thread: *Thread) void {
    if (current_thread == null) {
        current_thread = thread;
    } else {
        @panic("can't set current thread twice, silly!");
    }
}
