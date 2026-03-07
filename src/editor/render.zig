const std = @import("std");
const terminal = @import("../terminal.zig");
const Editor = @import("Editor.zig");

pub fn refreshScreen(self: *Editor) !void {
    self.scroll();

    self.render_buf.clearRetainingCapacity();
    const alloc = self.allocator;

    try self.render_buf.appendSlice(alloc, "\x1b[?25l");
    try self.render_buf.appendSlice(alloc, "\x1b[H");

    try drawRows(self);
    try drawStatusBar(self);
    try drawMessageBar(self);

    // Position cursor
    const cursor_row = self.cy - self.row_offset + 1;
    const cursor_col = self.rx - self.col_offset + 1;
    var pos_buf: [32]u8 = undefined;
    const pos = std.fmt.bufPrint(&pos_buf, "\x1b[{d};{d}H", .{ cursor_row, cursor_col }) catch unreachable;
    try self.render_buf.appendSlice(alloc, pos);

    try self.render_buf.appendSlice(alloc, "\x1b[?25h");
    try terminal.write(self.render_buf.items);
}

fn drawRows(self: *Editor) !void {
    const alloc = self.allocator;

    for (0..self.screen_rows) |y| {
        const file_row = y + self.row_offset;
        if (file_row < self.rows.items.len) {
            try appendVisibleRow(self, file_row);
        } else if (self.rows.items.len <= 1 and self.text.getTotalLength() == 0 and y == self.screen_rows / 3) {
            // Welcome message
            const welcome = "eko editor -- ^S=save ^F=find ^Q=quit";
            const wlen = @min(welcome.len, self.screen_cols);
            const padding = if (self.screen_cols > wlen) (self.screen_cols - wlen) / 2 else 0;
            if (padding > 0) {
                try self.render_buf.append(alloc, '~');
                for (1..padding) |_| try self.render_buf.append(alloc, ' ');
            }
            try self.render_buf.appendSlice(alloc, welcome[0..wlen]);
        } else {
            try self.render_buf.append(alloc, '~');
        }
        try self.render_buf.appendSlice(alloc, "\x1b[K");
        try self.render_buf.appendSlice(alloc, "\r\n");
    }
}

fn appendVisibleRow(self: *Editor, row_idx: usize) !void {
    const alloc = self.allocator;
    const row = self.rows.items[row_idx];

    if (row.size == 0) return;

    // Read the row content
    var line_storage: [8192]u8 = undefined;
    var line: []u8 = undefined;
    var heap_line: ?[]u8 = null;
    defer if (heap_line) |hl| self.allocator.free(hl);

    if (row.size <= line_storage.len) {
        self.text.copyRange(row.off, line_storage[0..row.size]);
        line = line_storage[0..row.size];
    } else {
        heap_line = try self.allocator.alloc(u8, row.size);
        self.text.copyRange(row.off, heap_line.?);
        line = heap_line.?;
    }

    // Expand tabs and compute rendered content
    var render_col: usize = 0;
    for (line) |c| {
        if (c == '\t') {
            const spaces = self.tab_size - (render_col % self.tab_size);
            for (0..spaces) |_| {
                if (render_col >= self.col_offset and render_col < self.col_offset + self.screen_cols) {
                    try self.render_buf.append(alloc, ' ');
                }
                render_col += 1;
            }
        } else {
            if (render_col >= self.col_offset and render_col < self.col_offset + self.screen_cols) {
                try self.render_buf.append(alloc, c);
            }
            render_col += 1;
        }
        if (render_col >= self.col_offset + self.screen_cols) break;
    }
}

fn drawStatusBar(self: *Editor) !void {
    const alloc = self.allocator;
    try self.render_buf.appendSlice(alloc, "\x1b[7m");

    const name = if (self.filename) |f| f else @as([]const u8, "[No Name]");
    const modified: []const u8 = if (self.dirty > 0) " (modified)" else "";

    var status_buf: [256]u8 = undefined;
    const left = std.fmt.bufPrint(&status_buf, " {s}{s} - {d} lines", .{ name, modified, self.rows.items.len }) catch "[status]";
    const left_len = @min(left.len, self.screen_cols);
    try self.render_buf.appendSlice(alloc, left[0..left_len]);

    var right_buf: [64]u8 = undefined;
    const right = std.fmt.bufPrint(&right_buf, "{d}/{d} ", .{ self.cy + 1, self.rows.items.len }) catch "";

    var col: usize = left_len;
    while (col < self.screen_cols) : (col += 1) {
        if (self.screen_cols - col == right.len) {
            try self.render_buf.appendSlice(alloc, right);
            break;
        }
        try self.render_buf.append(alloc, ' ');
    }
    try self.render_buf.appendSlice(alloc, "\x1b[m");
    try self.render_buf.appendSlice(alloc, "\r\n");
}

fn drawMessageBar(self: *Editor) !void {
    const alloc = self.allocator;
    try self.render_buf.appendSlice(alloc, "\x1b[K");

    if (self.status_msg_len > 0) {
        const now = std.time.timestamp();
        if (now - self.status_time < 5) {
            const len = @min(self.status_msg_len, self.screen_cols);
            try self.render_buf.appendSlice(alloc, self.status_msg[0..len]);
        }
    }
}
