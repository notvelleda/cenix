const std = @import("std");
const arch = @import("arch/i386.zig");
const mm = @import("mm.zig");
const process = @import("process.zig");

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

    const stack2 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_2.context = arch.Context.fromFn(@ptrCast(&thread2), @ptrCast(&stack2[stack_size - 1]));

    const stack3 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_3.context = arch.Context.fromFn(@ptrCast(&thread3), @ptrCast(&stack3[stack_size - 1]));

    const stack4 = mm.alloc(stack_size) catch @panic("allocation failed");
    thread_4.context = arch.Context.fromFn(@ptrCast(&thread4), @ptrCast(&stack4[stack_size - 1]));
    thread_4.next = &thread_1;

    mm.listBlocks();

    process.setCurrentThread(&thread_1);

    while (true) {
        std.log.info("thread 1", .{});
        waitAWhile();
        yield();
    }
}

export fn thread2() callconv(.C) void {
    while (true) {
        std.log.info("thread 2!", .{});
        waitAWhile();
        yield();
    }
}

export fn thread3() callconv(.C) void {
    while (true) {
        std.log.info("thread 3!!", .{});
        waitAWhile();
        yield();
    }
}

export fn thread4() callconv(.C) void {
    while (true) {
        std.log.info("thread 4!!!", .{});
        waitAWhile();
        yield();
    }
}

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
    asm volatile ("int $0x80");
}

fn waitAWhile() void {
    for (0..131072) |i| {
        _ = i;
        asm volatile ("pause");
        asm volatile ("pause");
        asm volatile ("pause");
        asm volatile ("pause");
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
