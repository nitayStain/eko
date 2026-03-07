const std = @import("std");
const Editor = @import("Editor.zig");
const input = @import("input.zig");

pub fn find(self: *Editor) !void {
    const saved_cx = self.cx;
    const saved_cy = self.cy;
    const saved_col_offset = self.col_offset;
    const saved_row_offset = self.row_offset;

    var query_buf = std.ArrayList(u8).empty;
    defer query_buf.deinit(self.allocator);

    var last_match_row: ?usize = null;
    var last_match_col: ?usize = null;

    while (true) {
        var msg_buf: [80]u8 = undefined;
        const msg = std.fmt.bufPrint(&msg_buf, "Search: {s} (ESC=cancel Enter=next)", .{query_buf.items}) catch query_buf.items;
        @memcpy(self.status_msg[0..msg.len], msg);
        self.status_msg_len = msg.len;
        self.status_time = std.time.timestamp();
        try self.refreshScreen();

        const key = try input.readKey();
        switch (key) {
            .char => |c| {
                if (c == '\x1b') {
                    // Restore position
                    self.cx = saved_cx;
                    self.cy = saved_cy;
                    self.col_offset = saved_col_offset;
                    self.row_offset = saved_row_offset;
                    self.setStatusMessage("", .{});
                    return;
                } else if (c == '\r') {
                    // Find next
                    findNext(self, query_buf.items, &last_match_row, &last_match_col, 1);
                } else if (c == 127) {
                    // Backspace in search
                    if (query_buf.items.len > 0) {
                        query_buf.items.len -= 1;
                        last_match_row = null;
                        last_match_col = null;
                        findNext(self, query_buf.items, &last_match_row, &last_match_col, 1);
                    }
                } else if (c >= 32 and c < 127) {
                    try query_buf.append(self.allocator, c);
                    last_match_row = null;
                    last_match_col = null;
                    findNext(self, query_buf.items, &last_match_row, &last_match_col, 1);
                }
            },
            .arrow_right, .arrow_down => {
                findNext(self, query_buf.items, &last_match_row, &last_match_col, 1);
            },
            .arrow_left, .arrow_up => {
                findNext(self, query_buf.items, &last_match_row, &last_match_col, -1);
            },
            else => {},
        }
    }
}

fn findNext(self: *Editor, query: []const u8, last_row: *?usize, last_col: *?usize, direction: i32) void {
    if (query.len == 0 or self.rows.items.len == 0) return;

    var start_row: usize = undefined;
    var start_col_signed: isize = undefined;

    if (last_row.*) |lr| {
        start_row = lr;
        if (last_col.*) |lc| {
            if (direction == 1) {
                start_col_signed = @as(isize, @intCast(lc)) + 1;
            } else {
                start_col_signed = @as(isize, @intCast(lc)) - 1;
            }
        } else {
            start_col_signed = 0;
        }
    } else {
        start_row = self.cy;
        start_col_signed = if (direction == 1) 0 else @as(isize, @intCast(self.rows.items[self.cy].size));
    }

    var line_buf: [8192]u8 = undefined;

    for (0..self.rows.items.len) |i| {
        var r: usize = undefined;
        if (direction == 1) {
            r = (start_row + i) % self.rows.items.len;
        } else {
            r = (start_row + self.rows.items.len - i) % self.rows.items.len;
        }

        const row = self.rows.items[r];
        if (row.size < query.len) continue;
        const read_len = @min(row.size, line_buf.len);
        if (read_len < query.len) continue;
        self.text.copyRange(row.off, line_buf[0..read_len]);
        const line = line_buf[0..read_len];

        var from: usize = 0;
        var to: usize = read_len;

        if (i == 0) {
            if (direction == 1) {
                from = if (start_col_signed >= 0) @intCast(start_col_signed) else 0;
            } else {
                to = if (start_col_signed >= 0 and start_col_signed + @as(isize, @intCast(query.len)) <= @as(isize, @intCast(read_len)))
                    @intCast(@as(isize, @intCast(start_col_signed)) + @as(isize, @intCast(query.len)))
                else
                    read_len;
            }
        }
        if (from > read_len) from = read_len;
        if (to > read_len) to = read_len;

        if (direction == 1) {
            var col = from;
            while (col + query.len <= to) : (col += 1) {
                if (std.mem.eql(u8, line[col .. col + query.len], query)) {
                    last_row.* = r;
                    last_col.* = col;
                    self.cy = r;
                    self.cx = col;
                    self.row_offset = self.rows.items.len;
                    return;
                }
            }
        } else {
            if (to >= query.len) {
                var col_plus: usize = to - query.len + 1;
                while (col_plus > from) {
                    col_plus -= 1;
                    if (std.mem.eql(u8, line[col_plus .. col_plus + query.len], query)) {
                        last_row.* = r;
                        last_col.* = col_plus;
                        self.cy = r;
                        self.cx = col_plus;
                        self.row_offset = self.rows.items.len;
                        return;
                    }
                }
            }
        }
    }
}

pub fn findReplace(self: *Editor) !void {
    const query = self.prompt("Find: {s} (ESC to cancel)", .{}) catch return;
    if (query == null) return;
    const q = query.?;
    defer self.allocator.free(q);

    const replacement = self.prompt("Replace with: {s} (ESC to cancel)", .{}) catch return;
    if (replacement == null) return;
    const repl = replacement.?;
    defer self.allocator.free(repl);

    var replaced: usize = 0;
    var last_row: ?usize = null;
    var last_col: ?usize = null;

    findNext(self, q, &last_row, &last_col, 1);
    if (last_row == null) {
        self.setStatusMessage("No matches found.", .{});
        return;
    }

    while (last_row != null) {
        self.setStatusMessage("Replace? Enter=yes Tab=skip ESC=done ({d} replaced)", .{replaced});
        try self.refreshScreen();

        const key = try input.readKey();
        switch (key) {
            .char => |c| {
                if (c == '\x1b') {
                    break;
                } else if (c == '\r') {
                    // Replace
                    const r = last_row.?;
                    const col = last_col.?;
                    const off = self.rows.items[r].off + col;
                    try self.text.delete(off, q.len);
                    if (repl.len > 0) {
                        try self.text.insert(off, repl);
                    }
                    try self.rebuildRows();
                    self.setCursorFromOffset(off + repl.len);
                    replaced += 1;
                    self.dirty += 1;

                    last_row = null;
                    last_col = null;
                    findNext(self, q, &last_row, &last_col, 1);
                    if (last_row == null) {
                        self.setStatusMessage("Replaced {d} occurrence(s). No more matches.", .{replaced});
                        break;
                    }
                } else if (c == '\t') {
                    findNext(self, q, &last_row, &last_col, 1);
                    if (last_row == null) {
                        self.setStatusMessage("No more matches. {d} replaced.", .{replaced});
                        break;
                    }
                }
            },
            else => {},
        }
    }
}

pub fn gotoLine(self: *Editor) !void {
    const result = self.prompt("Go to line: {s} (ESC to cancel)", .{}) catch return;
    if (result == null) return;
    const input_str = result.?;
    defer self.allocator.free(input_str);

    const line = std.fmt.parseInt(usize, input_str, 10) catch {
        self.setStatusMessage("Invalid line number.", .{});
        return;
    };

    if (line == 0) {
        self.cy = 0;
    } else if (line > self.rows.items.len) {
        self.cy = self.rows.items.len - 1;
    } else {
        self.cy = line - 1;
    }
    self.cx = 0;
    self.row_offset = self.rows.items.len; // force scroll recalculation
    self.setStatusMessage("Line {d}", .{self.cy + 1});
}
