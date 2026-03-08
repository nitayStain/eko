// Ported from https://github.com/daurnimator/zig-piecetable
// Adapted for Zig 0.15 unmanaged ArrayList API

const std = @import("std");

const PieceTable = @This();

base: []const u8,
add_buffer: std.ArrayList(u8) = .empty,
entries: std.ArrayList(Entry) = .empty,
allocator: std.mem.Allocator,
total_len: Pos = 0,

const Pos = usize;
pub const Entry = struct {
    from: Pos,
    len: Pos,

    const BufferType = enum { base, add };
    fn bufferType(entry: Entry, pt: PieceTable) BufferType {
        if (entry.from < pt.base.len) {
            return .base;
        } else {
            return .add;
        }
    }

    fn bufferSlice(entry: Entry, pt: PieceTable) []const u8 {
        return switch (entry.bufferType(pt)) {
            .base => pt.base[entry.from..],
            .add => pt.add_buffer.items[entry.from - pt.base.len ..],
        }[0..entry.len];
    }

    fn pointsAtLastAdded(entry: Entry, pt: PieceTable) bool {
        return entry.bufferType(pt) == .add and
            entry.from - pt.base.len + entry.len == pt.add_buffer.items.len;
    }
};

pub fn init(allocator: std.mem.Allocator, base: []const u8) error{OutOfMemory}!@This() {
    var pt = PieceTable{
        .base = base,
        .allocator = allocator,
    };
    if (base.len > 0) {
        try pt.entries.append(allocator, .{ .from = 0, .len = base.len });
        pt.total_len = base.len;
    }
    return pt;
}

pub fn deinit(self: *@This()) void {
    self.entries.deinit(self.allocator);
    self.add_buffer.deinit(self.allocator);
}

pub fn getTotalLength(self: @This()) Pos {
    return self.total_len;
}

const FindResult = struct {
    start: Pos,
    e: ?usize,
};
fn findEntry(self: @This(), index: Pos) FindResult {
    var a: Pos = 0;
    for (self.entries.items, 0..) |e, i| {
        if (index < a + e.len) {
            return .{
                .start = a,
                .e = i,
            };
        }
        a += e.len;
    }
    return .{
        .start = a,
        .e = null,
    };
}

fn getSlice(self: @This(), index: Pos) error{OutOfBounds}![]const u8 {
    const indexedEntry = self.findEntry(index);
    const entry_index = indexedEntry.e orelse return error.OutOfBounds;
    const entry = self.entries.items[entry_index];
    return entry.bufferSlice(self)[index - indexedEntry.start ..];
}

pub fn get(self: @This(), index: Pos) error{OutOfBounds}!u8 {
    return (try self.getSlice(index))[0];
}

pub fn append(self: *@This(), bytes: []const u8) error{OutOfMemory}!void {
    try self.add_buffer.ensureUnusedCapacity(self.allocator, bytes.len);

    if (self.entries.items.len > 0) {
        const last_entry = &self.entries.items[self.entries.items.len - 1];
        if (last_entry.pointsAtLastAdded(self.*)) {
            last_entry.len += bytes.len;
            self.add_buffer.appendSliceAssumeCapacity(bytes);
            self.total_len += bytes.len;
            return;
        }
    }

    try self.entries.append(self.allocator, .{
        .from = self.base.len + self.add_buffer.items.len,
        .len = bytes.len,
    });

    self.add_buffer.appendSliceAssumeCapacity(bytes);
    self.total_len += bytes.len;
}

pub fn insert(self: *@This(), index: Pos, bytes: []const u8) error{ OutOfBounds, OutOfMemory }!void {
    try self.add_buffer.ensureUnusedCapacity(self.allocator, bytes.len);

    const new_entry: Entry = .{
        .from = self.base.len + self.add_buffer.items.len,
        .len = bytes.len,
    };

    const indexedEntry = self.findEntry(index);
    if (indexedEntry.e) |entry_index| {
        if (indexedEntry.start == index) {
            if (!blk: {
                if (entry_index == 0) break :blk false;
                const previous_entry = &self.entries.items[entry_index - 1];
                if (previous_entry.pointsAtLastAdded(self.*)) {
                    previous_entry.len += bytes.len;
                    break :blk true;
                } else {
                    break :blk false;
                }
            }) {
                try self.entries.insert(self.allocator, entry_index, new_entry);
            }
        } else {
            const split_point = index - indexedEntry.start;
            const old_entry = self.entries.items[entry_index];
            try self.entries.replaceRange(self.allocator, entry_index, 1, &[3]Entry{
                .{
                    .from = old_entry.from,
                    .len = split_point,
                },
                new_entry,
                .{
                    .from = old_entry.from + split_point,
                    .len = old_entry.len - split_point,
                },
            });
        }
    } else if (indexedEntry.start == index) {
        try self.entries.append(self.allocator, new_entry);
    } else {
        return error.OutOfBounds;
    }

    self.add_buffer.appendSliceAssumeCapacity(bytes);
    self.total_len += bytes.len;
}

pub fn delete(self: *@This(), index: Pos, length: Pos) error{ OutOfBounds, OutOfMemory }!void {
    const indexedEntry = self.findEntry(index);
    if (indexedEntry.e) |start_entry_index| {
        const split_point = index - indexedEntry.start;

        var entry_index = start_entry_index;
        var length_to_delete = length;

        if (split_point != 0) {
            var start_entry = &self.entries.items[start_entry_index];
            const available_to_delete = start_entry.len - split_point;
            if (available_to_delete > length_to_delete) {
                try self.entries.insert(self.allocator, entry_index + 1, .{
                    .from = start_entry.from + split_point + length_to_delete,
                    .len = available_to_delete - length_to_delete,
                });
                start_entry = &self.entries.items[start_entry_index];
                start_entry.len = split_point;
                self.total_len -= length;
                return;
            }
            length_to_delete -= available_to_delete;
            start_entry.len = split_point;
            entry_index += 1;
        }

        var entries_to_delete: usize = 0;
        while (entry_index + entries_to_delete < self.entries.items.len) {
            const e = self.entries.items[entry_index + entries_to_delete];
            if (length_to_delete <= e.len) break;
            entries_to_delete += 1;
            length_to_delete -= e.len;
        }
        if (entries_to_delete > 0) {
            self.entries.replaceRangeAssumeCapacity(entry_index, entries_to_delete, &[0]Entry{});
        }

        if (length_to_delete != 0) {
            if (entry_index < self.entries.items.len) {
                const trim_entry = &self.entries.items[entry_index];
                trim_entry.from += length_to_delete;
                trim_entry.len -= length_to_delete;
            }
        }
        self.total_len -= length;
    } else {
        return error.OutOfBounds;
    }
}

/// Append a range of logical content directly into an output ArrayList,
/// avoiding temporary allocations.
pub fn appendRangeTo(self: @This(), start: Pos, len: Pos, out: *std.ArrayList(u8), allocator: std.mem.Allocator) !void {
    if (len == 0) return;
    try out.ensureUnusedCapacity(allocator, len);

    var remaining = len;
    var offset = start;
    var logical: Pos = 0;

    for (self.entries.items) |entry| {
        if (remaining == 0) break;
        const entry_end = logical + entry.len;

        if (offset >= entry_end) {
            logical = entry_end;
            continue;
        }

        const slice = entry.bufferSlice(self);
        const skip = offset - logical;
        const available = slice[skip..];
        const to_copy = @min(available.len, remaining);
        out.appendSliceAssumeCapacity(available[0..to_copy]);
        remaining -= to_copy;
        offset += to_copy;
        logical = entry_end;
    }
}

/// Copy a range of the logical content into `dst`.
pub fn copyRange(self: @This(), start: Pos, dst: []u8) void {
    var remaining = dst;
    var offset = start;

    var logical: Pos = 0;
    for (self.entries.items) |entry| {
        if (remaining.len == 0) break;
        const entry_end = logical + entry.len;

        if (offset >= entry_end) {
            logical = entry_end;
            continue;
        }

        const slice = entry.bufferSlice(self);
        const skip = offset - logical;
        const available = slice[skip..];
        const to_copy = @min(available.len, remaining.len);
        @memcpy(remaining[0..to_copy], available[0..to_copy]);
        remaining = remaining[to_copy..];
        offset += to_copy;
        logical = entry_end;
    }
}

test "PieceTable" {
    var pt = try PieceTable.init(std.testing.allocator, "example content");
    defer pt.deinit();

    try std.testing.expectEqual(@as(u8, 'e'), try pt.get(0));
    try std.testing.expectEqual(@as(usize, 15), pt.getTotalLength());

    try pt.append(". this");
    try std.testing.expectEqual(@as(usize, 21), pt.getTotalLength());

    try pt.append(" was appended..");
    try std.testing.expectEqual(@as(usize, 36), pt.getTotalLength());

    try pt.delete(35, 1);
    try std.testing.expectEqual(@as(usize, 35), pt.getTotalLength());

    try pt.insert(0, "Some ");
    try std.testing.expectEqual(@as(usize, 40), pt.getTotalLength());

    try pt.delete(5, 8);
    try std.testing.expectEqual(@as(usize, 32), pt.getTotalLength());
}
