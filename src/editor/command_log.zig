const std = @import("std");
const Editor = @import("Editor.zig");

pub const Entry = struct {
    kind: enum { insert, delete },
    offset: usize,
    bytes: []u8,
    cx: usize,
    cy: usize,
};

const Stack = std.ArrayList(Entry);

fn freeEntry(alloc: std.mem.Allocator, entry: Entry) void {
    alloc.free(entry.bytes);
}

fn clearStack(alloc: std.mem.Allocator, stack: *Stack) void {
    for (stack.items) |entry| freeEntry(alloc, entry);
    stack.clearRetainingCapacity();
}

pub fn deinitStacks(self: *Editor) void {
    clearStack(self.allocator, &self.undo_stack);
    self.undo_stack.deinit(self.allocator);
    clearStack(self.allocator, &self.redo_stack);
    self.redo_stack.deinit(self.allocator);
}

pub fn recordInsert(self: *Editor, offset: usize, bytes: []const u8) void {
    const alloc = self.allocator;
    clearStack(alloc, &self.redo_stack);

    if (self.undo_stack.items.len > 0 and self.cmd_seq == self.cmd_last_seq) {
        const last = &self.undo_stack.items[self.undo_stack.items.len - 1];
        if (last.kind == .insert and offset == last.offset + last.bytes.len and bytes.len == 1) {
            const new = alloc.realloc(last.bytes, last.bytes.len + 1) catch return;
            new[new.len - 1] = bytes[0];
            last.bytes = new;
            return;
        }
    }

    const saved = alloc.dupe(u8, bytes) catch return;
    self.undo_stack.append(alloc, .{
        .kind = .insert,
        .offset = offset,
        .bytes = saved,
        .cx = self.cx,
        .cy = self.cy,
    }) catch {
        alloc.free(saved);
    };
    self.cmd_last_seq = self.cmd_seq;
}

pub fn recordDelete(self: *Editor, offset: usize, bytes: []const u8) void {
    const alloc = self.allocator;
    clearStack(alloc, &self.redo_stack);

    if (self.undo_stack.items.len > 0 and self.cmd_seq == self.cmd_last_seq and bytes.len == 1) {
        const last = &self.undo_stack.items[self.undo_stack.items.len - 1];
        if (last.kind == .delete and offset + 1 == last.offset) {
            const new = alloc.alloc(u8, last.bytes.len + 1) catch return;
            new[0] = bytes[0];
            @memcpy(new[1..], last.bytes);
            alloc.free(last.bytes);
            last.bytes = new;
            last.offset = offset;
            last.cx = self.cx;
            last.cy = self.cy;
            return;
        } else if (last.kind == .delete and offset == last.offset) {
            const new = alloc.realloc(last.bytes, last.bytes.len + 1) catch return;
            new[new.len - 1] = bytes[0];
            last.bytes = new;
            return;
        }
    }

    const saved = alloc.dupe(u8, bytes) catch return;
    self.undo_stack.append(alloc, .{
        .kind = .delete,
        .offset = offset,
        .bytes = saved,
        .cx = self.cx,
        .cy = self.cy,
    }) catch {
        alloc.free(saved);
    };
    self.cmd_last_seq = self.cmd_seq;
}

pub fn breakSeq(self: *Editor) void {
    self.cmd_seq +%= 1;
}

pub fn undo(self: *Editor) !void {
    if (self.undo_stack.items.len == 0) {
        self.setStatusMessage("Nothing to undo", .{});
        return;
    }
    const entry = self.undo_stack.pop().?;
    const alloc = self.allocator;

    switch (entry.kind) {
        .insert => try self.text.delete(entry.offset, entry.bytes.len),
        .delete => try self.text.insert(entry.offset, entry.bytes),
    }

    try self.rebuildRows();
    self.updateLineNumWidth();
    self.updateSyntaxState();
    self.cy = entry.cy;
    self.cx = entry.cx;
    self.clampCursor();
    self.dirty += 1;

    self.redo_stack.append(alloc, entry) catch freeEntry(alloc, entry);
    self.cmd_seq +%= 1;
    self.cmd_last_seq = self.cmd_seq;
}

pub fn redo(self: *Editor) !void {
    if (self.redo_stack.items.len == 0) {
        self.setStatusMessage("Nothing to redo", .{});
        return;
    }
    const entry = self.redo_stack.pop().?;
    const alloc = self.allocator;

    switch (entry.kind) {
        .insert => try self.text.insert(entry.offset, entry.bytes),
        .delete => try self.text.delete(entry.offset, entry.bytes.len),
    }

    try self.rebuildRows();
    self.updateLineNumWidth();
    self.updateSyntaxState();
    switch (entry.kind) {
        .insert => self.setCursorFromOffset(entry.offset + entry.bytes.len),
        .delete => self.setCursorFromOffset(entry.offset),
    }
    self.dirty += 1;

    self.undo_stack.append(alloc, entry) catch freeEntry(alloc, entry);
    self.cmd_seq +%= 1;
    self.cmd_last_seq = self.cmd_seq;
}
