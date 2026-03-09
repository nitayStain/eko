const std = @import("std");
const Editor = @import("Editor.zig");

fn getSelOffsets(self: *const Editor) ?struct { start: usize, end: usize } {
    const sel = self.selection orelse return null;

    const a_off = if (sel.mark_y < self.rows.items.len)
        self.rows.items[sel.mark_y].off + sel.mark_x
    else
        self.text.getTotalLength();

    const b_off = self.cursorOffset();

    if (a_off <= b_off) {
        return .{ .start = a_off, .end = b_off };
    } else {
        return .{ .start = b_off, .end = a_off };
    }
}

pub fn selectStart(self: *Editor) void {
    if (self.selection == null) {
        self.selection = .{
            .mark_x = self.cx,
            .mark_y = self.cy,
        };
    }
    self.markDirtyAll();
}

pub fn selectClear(self: *Editor) void {
    if (self.selection != null) self.markDirtyAll();
    self.selection = null;
}

pub fn deleteSelection(self: *Editor) !void {
    const offsets = getSelOffsets(self) orelse return;
    const len = offsets.end - offsets.start;
    if (len == 0) {
        self.selection = null;
        return;
    }

    self.breakSeq();
    const saved = self.allocator.alloc(u8, len) catch null;
    if (saved) |buf| {
        self.text.copyRange(offsets.start, buf);
        self.recordDelete(offsets.start, buf);
        self.allocator.free(buf);
    }
    try self.text.delete(offsets.start, len);
    try self.rebuildRows();
    self.setCursorFromOffset(offsets.start);
    self.selection = null;
    self.dirty += 1;
}

pub fn copy(self: *Editor) void {
    const offsets = getSelOffsets(self) orelse return;
    const len = offsets.end - offsets.start;
    if (len == 0) return;

    if (self.clipboard) |old| self.allocator.free(old);
    self.clipboard = self.allocator.alloc(u8, len) catch return;
    self.text.copyRange(offsets.start, self.clipboard.?);

    pbcopyWrite(self.clipboard.?);

    self.copy_flash_off = .{ .start = offsets.start, .end = offsets.end };
    self.copy_flash_start = std.time.milliTimestamp();

    self.setStatusMessage("Copied {d} bytes", .{len});
}

pub fn cut(self: *Editor) !void {
    const offsets = getSelOffsets(self) orelse return;
    const len = offsets.end - offsets.start;
    if (len == 0) return;

    if (self.clipboard) |old| self.allocator.free(old);
    self.clipboard = try self.allocator.alloc(u8, len);
    self.text.copyRange(offsets.start, self.clipboard.?);

    pbcopyWrite(self.clipboard.?);

    self.breakSeq();
    self.recordDelete(offsets.start, self.clipboard.?);
    try self.text.delete(offsets.start, len);
    try self.rebuildRows();
    self.setCursorFromOffset(offsets.start);
    self.selection = null;
    self.dirty += 1;
    self.setStatusMessage("Cut {d} bytes", .{len});
}

pub fn paste(self: *Editor) !void {
    if (self.selection != null) try self.deleteSelection();

    // Try to read from system clipboard via pbpaste
    const sys_clip = pbpasteRead(self.allocator);
    if (sys_clip) |clip| {
        if (self.clipboard) |old| self.allocator.free(old);
        self.clipboard = clip;
    }

    const clip = self.clipboard orelse return;
    if (clip.len == 0) return;

    if (self.cy >= self.rows.items.len) return;

    const off = self.cursorOffset();
    self.breakSeq();
    self.recordInsert(off, clip);
    try self.text.insert(off, clip);
    try self.rebuildRows();
    self.setCursorFromOffset(off + clip.len);
    self.dirty += 1;
    self.setStatusMessage("Pasted {d} bytes", .{clip.len});
}

fn pbcopyWrite(data: []const u8) void {
    var child = std.process.Child.init(&.{"pbcopy"}, std.heap.page_allocator);
    child.stdin_behavior = .Pipe;
    child.stdout_behavior = .Close;
    child.stderr_behavior = .Close;
    child.spawn() catch return;
    if (child.stdin) |*stdin| {
        stdin.writeAll(data) catch {};
        stdin.close();
        child.stdin = null;
    }
    _ = child.wait() catch {};
}

fn pbpasteRead(allocator: std.mem.Allocator) ?[]u8 {
    var child = std.process.Child.init(&.{"pbpaste"}, allocator);
    child.stdout_behavior = .Pipe;
    child.stdin_behavior = .Close;
    child.stderr_behavior = .Close;
    child.spawn() catch return null;

    const stdout = child.stdout orelse {
        _ = child.wait() catch {};
        return null;
    };
    const result = stdout.readToEndAlloc(allocator, 1024 * 1024) catch {
        _ = child.wait() catch {};
        return null;
    };
    _ = child.wait() catch {};

    if (result.len == 0) {
        allocator.free(result);
        return null;
    }
    return result;
}
