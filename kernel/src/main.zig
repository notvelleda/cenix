const std = @import("std");
const arch = @import("arch/i386.zig");
const mm = @import("mm.zig");
const process = @import("process.zig");
const syscalls = @import("syscalls.zig");

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

    const stack_size = 0x1000 * 8;

    const stack1 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_1.context = arch.Context.fromFn(@ptrCast(&thread1), @ptrFromInt(@intFromPtr(stack1) + stack_size));

    const stack2 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_2.context = arch.Context.fromFn(@ptrCast(&thread2), @ptrFromInt(@intFromPtr(stack2) + stack_size));

    const stack3 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_3.context = arch.Context.fromFn(@ptrCast(&thread3), @ptrFromInt(@intFromPtr(stack3) + stack_size));

    const stack4 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_4.context = arch.Context.fromFn(@ptrCast(&thread4), @ptrFromInt(@intFromPtr(stack4) + stack_size));
    thread_4.next = &thread_1;

    mm.listBlocks();

    // hop out of ring 0 so that context switching actually works
    process.setCurrentThread(&thread_0);
    yield();
}

export fn thread1() callconv(.C) void {
    //asm volatile ("int $0x81");

    while (true) {
        //std.log.info("thread 1", .{});
        debugMsg("thread 1");
        waitAWhile();
        yield();
    }
}

export fn thread2() callconv(.C) void {
    while (true) {
        //std.log.info("thread 2!", .{});
        debugMsg("thread 2!");
        waitAWhile();
        yield();
    }
}

export fn thread3() callconv(.C) void {
    while (true) {
        //std.log.info("thread 3!!", .{});
        debugMsg("thread 3!!");
        waitAWhile();
        yield();
    }
}

export fn thread4() callconv(.C) void {
    while (true) {
        //std.log.info("thread 4!!!", .{});
        debugMsg("thread 4!!!");
        waitAWhile();
        yield();
    }
}

var thread_0 = process.Thread {
    .context = arch.Context.default(),
    .next = &thread_1,
};

var thread_1 = process.Thread {
    .context = arch.Context.default(),
    .next = &thread_2,
};

var thread_2 = process.Thread {
    .context = arch.Context.default(),
    .next = &thread_3,
};

var thread_3 = process.Thread {
    .context = arch.Context.default(),
    .next = &thread_4,
};

var thread_4 = process.Thread {
    .context = arch.Context.default(),
    .next = null,
};

fn yield() void {
    asm volatile (
        "int $0x80"
        :
        : [num] "{eax}" (syscalls.Syscalls.yield)
        : "eax", "ebx"
    );
}

fn debugMsg(s: []const u8) void {
    asm volatile (
        "int $0x80"
        :
        : [num] "{eax}" (syscalls.Syscalls.debugMessage), [addr] "{ebx}" (&s[0]), [len] "{ecx}" (s.len)
        : "eax", "ebx"
    );
}

fn waitAWhile() void {
    for (0..131072) |i| {
        _ = i;
        asm volatile ("pause; pause; pause; pause; pause; pause");
    }
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
