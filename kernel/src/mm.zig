const std = @import("std");

const log_scope = std.log.scoped(.mm);

const BlockKind = enum {
    Available,
    Immovable,
    Movable,
};

const Header = struct {
    size: usize,
    kind: BlockKind,
    next: ?*Header,
    prev: ?*Header,
};

var heap_base: *Header = undefined;
var total_memory: usize = 0;
var used_memory: usize = 0;

/// describes how the memory that the kernel is loaded in is laid out
pub const InitBlock = struct {
    /// a pointer to the start of the kernel (inclusive)
    kernel_start: *usize,
    /// a pointer to the end of the kernel (exclusive)
    kernel_end: *usize,
    /// a pointer to the start of a contiguous block of memory (inclusive)
    memory_start: *usize,
    /// a pointer to the end of the contiguous block of memory (exclusive)
    memory_end: *usize,
};

/// initializes the memory manager
pub fn init(init_block: InitBlock) void {
    log_scope.debug("initializing heap at {x:0>8} - {x:0>8} (kernel at {x:0>8} - {x:0>8})", .{
        @intFromPtr(init_block.memory_start),
        @intFromPtr(init_block.memory_end),
        @intFromPtr(init_block.kernel_start),
        @intFromPtr(init_block.kernel_end),
    });

    if (@intFromPtr(init_block.memory_start) >= @intFromPtr(init_block.kernel_start) or @intFromPtr(init_block.memory_end) <= @intFromPtr(init_block.kernel_end)) {
        var header: *Header = @ptrCast(init_block.memory_start);
        header.kind = .Available;
        header.size = @intFromPtr(init_block.memory_end) - @intFromPtr(init_block.memory_start);
        header.prev = null;
        header.next = null;

        heap_base = header;

        total_memory = header.size;
        used_memory = 0;
    } else {
        // TODO: properly handle heap at the very beginning or end of the memory block

        var kernel_header: *Header = @ptrFromInt(@intFromPtr(init_block.kernel_start) - @sizeOf(Header));
        kernel_header.kind = .Immovable;
        kernel_header.size = @intFromPtr(init_block.kernel_end) - @intFromPtr(kernel_header);

        var header_low: *Header = @ptrCast(init_block.memory_start);
        header_low.kind = .Available;
        header_low.size = @intFromPtr(kernel_header) - @intFromPtr(header_low);

        heap_base = header_low;

        var header_high: *Header = @ptrCast(init_block.kernel_end);
        header_high.kind = .Available;
        header_high.size = @intFromPtr(init_block.memory_end) - @intFromPtr(header_high);

        header_low.prev = null;
        header_low.next = kernel_header;

        kernel_header.prev = header_low;
        kernel_header.next = header_high;

        header_high.prev = kernel_header;
        header_high.next = null;

        total_memory = @intFromPtr(init_block.memory_end) - @intFromPtr(init_block.memory_start);
        used_memory = kernel_header.size;
    }

    log_scope.debug("total memory: {} KiB, used memory: {} KiB", .{total_memory / 1024, used_memory / 1024});
}

/// adds a contiguous block of usable memory to the heap
pub fn addMemoryBlock(start: *u8, end: *u8) void {
    log_scope.debug("adding memory to heap from {x:0>8} - {x:0>8}", .{@intFromPtr(start), @intFromPtr(end)});
}

pub const AllocError = error { OutOfMemory };

/// allocates a region of memory, returning a pointer to it. the newly allocated region of memory is set as locked (immovable)
pub fn alloc(actual_size: usize) AllocError!*u8 {
    var size = actual_size + @sizeOf(Header);
    log_scope.debug("alloc: size {} (adjusted to {})", .{actual_size, size});

    // search for a series of consecutive movable or available blocks big enough to fit the allocation

    var start_header = heap_base;
    var end_header = heap_base;
    var total_size = heap_base.size;
    var to_move: usize = 0;
    var available_memory = total_memory - used_memory;

    if (size > available_memory) {
        return AllocError.OutOfMemory;
    }

    while (true) {
        switch (end_header.kind) {
            .Available => available_memory -= end_header.size,
            .Movable => to_move += end_header.size,
            .Immovable => {
                if (end_header.next) |next| {
                    // start the search over from the block following this immovable block
                    start_header = next;
                    end_header = next;
                    total_size = end_header.size;
                    to_move = 0;
                    available_memory = total_memory - used_memory;
                    continue;
                } else {
                    return AllocError.OutOfMemory;
                }
            }
        }

        if (total_size >= size) {
            // found a potential series of blocks, make sure everything will work out
            if (to_move <= available_memory) {
                break;
            } else {
                // more data needs to be reallocated and moved around than there is available memory, attempt to find a different series of blocks
                start_header = end_header;
                total_size = end_header.size;
                to_move = 0;
                available_memory = total_memory - used_memory;
            }
        } else if (end_header.next) |next| {
            end_header = next;
            total_size += next.size;
        } else {
            return AllocError.OutOfMemory;
        }
    }

    log_scope.debug("alloc: moving 0x{x}, 0x{x} available", .{to_move, available_memory});

    // split end_header
    var split_pos = size - (total_size - end_header.size);

    if (split_pos >= end_header.size) {
        split_pos = 0;
    }

    // only split the end header if it's unallocated and if the resulting block in the area to be allocated can actually fit data in it
    // if the allocation fails that makes it easier to clean up
    if (split_pos > @sizeOf(Header) and end_header.kind == .Available) {
        log_scope.debug("alloc: splitting end_header at {}", .{split_pos});
        splitHeader(end_header, split_pos);
    }

    var end_header_movable = end_header.kind == .Movable;

    if (to_move > 0) {
        var header = start_header;
        while (header != end_header) : (header = header.next.?) {
            if (header.kind == .Movable) {
                var alloc_size = header.size - @sizeOf(Header);
                var dest_ptr: [*]u8 = @ptrFromInt(@intFromPtr(try alloc(alloc_size)));
                var src_ptr: [*]u8 = @ptrFromInt(@intFromPtr(header) + @sizeOf(Header));
                @memcpy(dest_ptr, src_ptr[0..alloc_size]);
                // this is faster than just calling free() since alloc() will automatically merge consecutive blocks
                header.kind = .Available;
                used_memory -= header.size;
                // TODO: notify whatever owns the newly moved block that it was moved
            }
        }
    }

    // since the contents of end_header have been properly moved, it can be split now
    if (split_pos > @sizeOf(Header) and end_header_movable) {
        log_scope.debug("alloc: splitting end_header at {}", .{split_pos});
        splitHeader(end_header, split_pos);
    }

    // set up the header and footer of the new allocation
    start_header.size = size;
    start_header.kind = .Immovable;
    start_header.next = end_header.next;
    if (start_header.next) |next| {
        next.prev = start_header;
    }

    used_memory += size;

    return @ptrFromInt(@intFromPtr(start_header) + @sizeOf(Header));
}

fn splitHeader(header: *Header, at: usize) void {
    // the header of the newly split block
    var new_header: *Header = @ptrFromInt(@intFromPtr(header) + at);
    new_header.size = header.size - at;
    new_header.kind = .Available;
    new_header.next = header.next;
    new_header.prev = header;

    if (new_header.next) |next| {
        next.prev = new_header;
    }

    header.size = at;
    header.next = new_header;
}

/// locks an allocated region of memory in place, allowing for any pointers to it to remain valid
pub fn lock(ptr: [*]u8) void {
    var header: *Header = @ptrFromInt(@intFromPtr(ptr) - @sizeOf(Header));

    header.kind = .Immovable;
}

/// unlocks an allocated region of memory, invalidating any existing pointers to it and allowing it to be moved anywhere else in memory if required
pub fn unlock(ptr: [*]u8) void {
    var header: *Header = @ptrFromInt(@intFromPtr(ptr) - @sizeOf(Header));

    header.kind = .Movable;
}

/// frees a region of memory, allowing it to be reused for other things
pub fn free(ptr: *u8) void {
    var header: *Header = @ptrFromInt(@intFromPtr(ptr) - @sizeOf(Header));

    header.kind = .Available;
    used_memory -= header.size;

    // check if the block directly after this one is available, and merge them if it is
    if (header.next) |next| {
        if (next.kind == .Available) {
            header.size += next.size;
            header.next = next.next;

            if (next.next) |next_next| {
                next_next.prev = header;
            }
        }
    }

    // merge with the block directly before if applicable
    if (header.prev) |prev| {
        if (prev.kind == .Available) {
            prev.size += header.size;
            prev.next = header.next;

            if (header.next) |next| {
                next.prev = prev;
            }
        }
    }
}

/// prints out a list of all the blocks in the heap
pub fn listBlocks() void {
    var header = heap_base;
    while (true) {
        log_scope.debug("{x:0>8} - {x:0>8} (size {x:0>8}): {}", .{@intFromPtr(header), @intFromPtr(header) + header.size, header.size, header.kind});

        if (header.next) |next| {
            header = next;
        } else {
            break;
        }
    }
}
