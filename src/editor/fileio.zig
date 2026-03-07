const std = @import("std");
const PieceTable = @import("../piece_table.zig");
const Editor = @import("Editor.zig");
const syntax_mod = @import("syntax.zig");

pub fn open(self: *Editor, path: []const u8) !void {
    if (self.filename) |old| self.allocator.free(old);
    self.filename = try self.allocator.dupe(u8, path);

    self.syntax = syntax_mod.selectSyntax(self.filename);

    const file = std.fs.cwd().openFile(path, .{}) catch |err| {
        if (err == error.FileNotFound) {
            self.setStatusMessage("New file: {s}", .{path});
            return;
        }
        return err;
    };
    defer file.close();

    const content = try file.readToEndAlloc(self.allocator, 10 * 1024 * 1024);

    self.text.deinit();
    if (self.base_buf) |old| self.allocator.free(old);

    self.base_buf = content;
    self.text = try PieceTable.init(self.allocator, content);
    try self.rebuildRows();
    self.updateSyntaxState();
    self.dirty = 0;
    self.cx = 0;
    self.cy = 0;
    self.row_offset = 0;
    self.col_offset = 0;
}

pub fn save(self: *Editor) !void {
    if (self.filename == null) {
        const name = self.prompt("Save as: {s} (ESC to cancel)", .{}) catch return;
        if (name) |n| {
            if (n.len == 0) {
                self.allocator.free(n);
                self.setStatusMessage("Save aborted.", .{});
                return;
            }
            if (self.filename) |old| self.allocator.free(old);
            self.filename = n;
        } else {
            self.setStatusMessage("Save aborted.", .{});
            return;
        }
    }

    const name = self.filename orelse return;
    const total = self.text.getTotalLength();
    const buf = try self.allocator.alloc(u8, total);
    defer self.allocator.free(buf);
    self.text.copyRange(0, buf);

    const file = std.fs.cwd().createFile(name, .{ .truncate = true }) catch {
        self.setStatusMessage("Can't save! I/O error", .{});
        return;
    };
    defer file.close();
    file.writeAll(buf) catch {
        self.setStatusMessage("Can't save! I/O error", .{});
        return;
    };
    if (total > 0 and buf[total - 1] != '\n') {
        file.writeAll("\n") catch {};
    }

    self.dirty = 0;
    self.setStatusMessage("{d} bytes written to disk", .{total});
}
