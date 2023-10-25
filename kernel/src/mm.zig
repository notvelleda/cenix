const std = @import("std");

const log_scope = std.log.scoped(.mm);

/// describes how the memory that the kernel is loaded in is laid out
pub const InitBlock = struct {
    /// a pointer to the start of the kernel (inclusive)
    kernel_start: *usize,
    /// a pointer to the end of the kernel (exclusive)
    kernel_end: *usize,
    /// a pointer to the start of the contiguous block of memory containing the kernel (inclusive)
    memory_start: *usize,
    /// a pointer to the end of the contiguous block of memory containing the kernel (exclusive)
    memory_end: *usize,
};

/// initializes the memory manager
pub fn init(init_block: InitBlock) void {
    log_scope.info("initializing heap at {x:0>8} - {x:0>8} (kernel at {x:0>8} - {x:0>8})", .{
        @intFromPtr(init_block.memory_start),
        @intFromPtr(init_block.memory_end),
        @intFromPtr(init_block.kernel_start),
        @intFromPtr(init_block.kernel_end),
    });

    // todo: checks if kernel is at the beginning/end of memory

    var header: *Header = @ptrCast(init_block.memory_start);
    var footer: *Footer = @ptrFromInt(@intFromPtr(init_block.memory_end) - @sizeOf(Footer));

    footer.header = header;
    header.kind = HoleKind.Available;
    header.size = @intFromPtr(init_block.memory_end) - @intFromPtr(init_block.memory_start);
}

/// adds a contiguous block of usable memory to the heap
pub fn addMemoryBlock(start: *u8, end: *u8) void {
    log_scope.info("adding memory to heap from {x:0>8} - {x:0>8}", .{@intFromPtr(start), @intFromPtr(end)});
}

const HoleKind = enum {
    Available,
    Unlocked,
    Locked,
};

const Header = struct {
    size: usize,
    kind: HoleKind,
};

const Footer = struct {
    header: *Header,
};
