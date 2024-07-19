const std = @import("std");
const arch = @import("arch/i386.zig");
const mm = @import("mm.zig");

const log_scope = std.log.scoped(.process);

const num_buckets = 128;
pub const ProcessTable = struct {
    buckets: [num_buckets]?*Thread,
    next_process: i16, // signed to allow for easy posix compatibility and to allow for negative process ids for kernel tasks
};

pub var process_table: *ProcessTable = undefined;

pub fn init() void {
    process_table = @alignCast(@ptrCast(mm.the_heap.alloc(@sizeOf(ProcessTable)) catch @panic("allocation failed")));

    for (0..num_buckets) |i| {
        process_table.buckets[i] = null;
    }
    process_table.next_process = 1;
}

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
        @panic("no current thread");
    }
}

pub fn setCurrentThread(thread: *Thread) void {
    if (current_thread == null) {
        current_thread = thread;
    } else {
        @panic("can't set current thread twice, silly!");
    }
}
