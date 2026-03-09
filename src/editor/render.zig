const std = @import("std");
const terminal = @import("../terminal.zig");
const Editor = @import("Editor.zig");
const syntax_mod = @import("syntax.zig");
const Color = Editor.Color;
const HlType = Editor.HlType;

pub fn refreshScreen(self: *Editor) !void {
    self.scroll();

    self.render_buf.clearRetainingCapacity();
    const alloc = self.allocator;

    try self.render_buf.appendSlice(alloc, "\x1b[?25l");

    // Set background color if configured
    const has_bg = switch (self.config.color_bg) {
        .none => false,
        else => true,
    };
    if (has_bg) {
        try self.config.color_bg.writeBg(&self.render_buf, alloc);
    }

    try drawRows(self);
    try drawStatusBar(self);
    try drawMessageBar(self);

    if (has_bg) {
        try self.render_buf.appendSlice(alloc, "\x1b[49m");
    }

    // Position cursor (accounting for line number gutter)
    const cursor_row = self.cy - self.row_offset + 1;
    const cursor_col = self.rx - self.col_offset + 1 + self.line_num_width;
    var pos_buf: [32]u8 = undefined;
    const pos = std.fmt.bufPrint(&pos_buf, "\x1b[{d};{d}H", .{ cursor_row, cursor_col }) catch unreachable;
    try self.render_buf.appendSlice(alloc, pos);

    try self.render_buf.appendSlice(alloc, "\x1b[?25h");
    try terminal.write(self.render_buf.items);
}

fn drawRows(self: *Editor) !void {
    const alloc = self.allocator;
    const gutter = self.line_num_width;
    const text_cols = self.textCols();
    const sd = self.screen_dirty;

    for (0..self.screen_rows) |y| {
        if (!self.dirty_all) {
            if (sd) |d| {
                if (y < d.len and !d[y]) continue;
            }
        }

        var row_pos_buf: [16]u8 = undefined;
        const row_pos = std.fmt.bufPrint(&row_pos_buf, "\x1b[{d};1H", .{y + 1}) catch unreachable;
        try self.render_buf.appendSlice(alloc, row_pos);

        const file_row = y + self.row_offset;
        if (file_row < self.rows.items.len) {
            // Line number gutter
            if (gutter > 0) {
                try self.render_buf.appendSlice(alloc, "\x1b[90m");
                var lnum_buf: [16]u8 = undefined;
                const lnum = std.fmt.bufPrint(&lnum_buf, "{d}", .{file_row + 1}) catch "?";
                // Right-align the number in (gutter-1) chars + 1 space
                const num_width = gutter - 1;
                if (lnum.len < num_width) {
                    for (0..num_width - lnum.len) |_| try self.render_buf.append(alloc, ' ');
                }
                try self.render_buf.appendSlice(alloc, lnum);
                try self.render_buf.append(alloc, ' ');
                try self.render_buf.appendSlice(alloc, "\x1b[39m");
            }

            try appendVisibleRowWithSyntax(self, file_row, text_cols);
        } else {
            // Gutter for empty rows
            if (gutter > 0) {
                try self.render_buf.appendSlice(alloc, "\x1b[90m");
                for (0..gutter) |_| try self.render_buf.append(alloc, ' ');
                try self.render_buf.appendSlice(alloc, "\x1b[39m");
            }

            if (self.rows.items.len <= 1 and self.text.getTotalLength() == 0 and y == self.screen_rows / 3) {
                const welcome = "eko editor -- ^S=save ^F=find ^Q=quit";
                const wlen = @min(welcome.len, text_cols);
                const padding = if (text_cols > wlen) (text_cols - wlen) / 2 else 0;
                if (padding > 0) {
                    try self.render_buf.append(alloc, '~');
                    for (1..padding) |_| try self.render_buf.append(alloc, ' ');
                }
                try self.render_buf.appendSlice(alloc, welcome[0..wlen]);
            } else {
                try self.render_buf.append(alloc, '~');
            }
        }
        try self.render_buf.appendSlice(alloc, "\x1b[K");

        if (sd) |d| {
            if (y < d.len) d[y] = false;
        }
    }

    self.dirty_all = false;
}

fn appendVisibleRowWithSyntax(self: *Editor, row_idx: usize, max_cols: usize) !void {
    const alloc = self.allocator;
    const row = self.rows.items[row_idx];

    if (row.size == 0) return;

    // Read raw row content
    var stack_buf: [8192]u8 = undefined;
    var heap_buf: ?[]u8 = null;
    defer if (heap_buf) |hb| self.allocator.free(hb);

    const raw: []u8 = if (row.size <= stack_buf.len)
        stack_buf[0..row.size]
    else blk: {
        heap_buf = try self.allocator.alloc(u8, row.size);
        break :blk heap_buf.?;
    };
    self.text.copyRange(row.off, raw);

    // Expand tabs into rendered buffer
    const tab_size = self.config.tab_size;
    var rendered_storage: [16384]u8 = undefined;
    var rendered_len: usize = 0;
    for (raw) |c| {
        if (c == '\t') {
            const spaces = tab_size - (rendered_len % tab_size);
            for (0..spaces) |_| {
                if (rendered_len < rendered_storage.len) {
                    rendered_storage[rendered_len] = ' ';
                    rendered_len += 1;
                }
            }
        } else {
            if (rendered_len < rendered_storage.len) {
                rendered_storage[rendered_len] = c;
                rendered_len += 1;
            }
        }
    }
    const rendered = rendered_storage[0..rendered_len];

    // Compute syntax highlights
    var hl_storage: [16384]HlType = undefined;
    const hl = hl_storage[0..rendered_len];
    @memset(hl, .normal);

    if (self.syntax) |syn| {
        _ = syntax_mod.computeHighlights(syn, rendered, hl, row.hl_open_comment);
    }

    // Compute selection/flash range in rendered coordinates
    var sel_start_rx: ?usize = null;
    var sel_end_rx: ?usize = null;
    var sel_color = self.config.color_selection;

    // Determine absolute offsets for the highlighted region
    var s_off: ?usize = null;
    var e_off: ?usize = null;

    if (self.copy_flash_off) |flash| {
        // Copy flash takes priority over selection
        s_off = flash.start;
        e_off = flash.end;
        sel_color = self.config.color_copied;
    } else if (self.selection) |sel| {
        const a_off = if (sel.mark_y < self.rows.items.len)
            self.rows.items[sel.mark_y].off + sel.mark_x
        else
            self.text.getTotalLength();
        const b_off = self.cursorOffset();
        s_off = @min(a_off, b_off);
        e_off = @max(a_off, b_off);
    }

    if (s_off != null and e_off != null) {
        const row_off = row.off;
        const row_end = row_off + row.size;
        if (s_off.? < row_end and e_off.? > row_off) {
            const sc = if (s_off.? > row_off) s_off.? - row_off else 0;
            const ec = if (e_off.? < row_end) e_off.? - row_off else row.size;
            sel_start_rx = self.rowCxToRx(row_idx, sc);
            sel_end_rx = self.rowCxToRx(row_idx, ec);
            if (sel_start_rx.? < self.col_offset) sel_start_rx = self.col_offset;
        }
    }

    const col_start = self.col_offset;
    const col_end = col_start + max_cols;
    var current_color: Color = .none;
    var in_sel = false;

    var j = col_start;
    while (j < @min(col_end, rendered_len)) : (j += 1) {
        const want_sel = (sel_start_rx != null and sel_end_rx != null and
            j >= sel_start_rx.? and j < sel_end_rx.?);

        if (want_sel and !in_sel) {
            try sel_color.writeBg(&self.render_buf, alloc);
            in_sel = true;
        } else if (!want_sel and in_sel) {
            switch (self.config.color_bg) {
                .none => try self.render_buf.appendSlice(alloc, "\x1b[49m"),
                else => try self.config.color_bg.writeBg(&self.render_buf, alloc),
            }
            in_sel = false;
        }

        const color = syntax_mod.hlToColor(hl[j], &self.config);
        if (!color.eql(current_color)) {
            current_color = color;
            switch (color) {
                .none => try self.render_buf.appendSlice(alloc, "\x1b[39m"),
                else => try color.writeFg(&self.render_buf, alloc),
            }
        }

        try self.render_buf.append(alloc, rendered[j]);
    }

    if (in_sel) {
        switch (self.config.color_bg) {
            .none => try self.render_buf.appendSlice(alloc, "\x1b[49m"),
            else => try self.config.color_bg.writeBg(&self.render_buf, alloc),
        }
    }
    switch (current_color) {
        .none => {},
        else => try self.render_buf.appendSlice(alloc, "\x1b[39m"),
    }
}

fn drawStatusBar(self: *Editor) !void {
    const alloc = self.allocator;

    var sb_pos_buf: [16]u8 = undefined;
    const sb_pos = std.fmt.bufPrint(&sb_pos_buf, "\x1b[{d};1H", .{self.screen_rows + 1}) catch unreachable;
    try self.render_buf.appendSlice(alloc, sb_pos);
    try self.render_buf.appendSlice(alloc, "\x1b[7m");

    const name: []const u8 = if (self.filename) |f| f else "[No Name]";
    const modified: []const u8 = if (self.dirty > 0) " (modified)" else "";

    var status_buf: [256]u8 = undefined;
    const left = std.fmt.bufPrint(&status_buf, " {s}{s} - {d} lines", .{ name, modified, self.rows.items.len }) catch "[status]";
    const left_len = @min(left.len, self.screen_cols);
    try self.render_buf.appendSlice(alloc, left[0..left_len]);

    var right_buf: [64]u8 = undefined;
    const ft: []const u8 = if (self.syntax) |s| s.filetype else "no ft";
    const right = std.fmt.bufPrint(&right_buf, "{s} | {d}/{d} ", .{ ft, self.cy + 1, self.rows.items.len }) catch "";

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

    var mb_pos_buf: [16]u8 = undefined;
    const mb_pos = std.fmt.bufPrint(&mb_pos_buf, "\x1b[{d};1H", .{self.screen_rows + 2}) catch unreachable;
    try self.render_buf.appendSlice(alloc, mb_pos);
    try self.render_buf.appendSlice(alloc, "\x1b[K");

    if (self.status_msg_len > 0) {
        const now = std.time.timestamp();
        if (now - self.status_time < 5) {
            const len = @min(self.status_msg_len, self.screen_cols);
            try self.render_buf.appendSlice(alloc, self.status_msg[0..len]);
        }
    }
}
