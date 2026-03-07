const Editor = @import("Editor.zig");

pub fn scroll(self: *Editor) void {
    updateLineNumWidth(self);

    self.rx = self.rowCxToRx(self.cy, self.cx);

    const text_cols = self.textCols();

    if (self.cy < self.row_offset) {
        self.row_offset = self.cy;
    }
    if (self.cy >= self.row_offset + self.screen_rows) {
        self.row_offset = self.cy - self.screen_rows + 1;
    }
    if (self.rx < self.col_offset) {
        self.col_offset = self.rx;
    }
    if (self.rx >= self.col_offset + text_cols) {
        self.col_offset = self.rx - text_cols + 1;
    }
}

pub fn moveCursor(self: *Editor, key: Editor.Key) void {
    switch (key) {
        .arrow_left => {
            if (self.cx > 0) {
                self.cx -= 1;
            } else if (self.cy > 0) {
                self.cy -= 1;
                self.cx = self.rows.items[self.cy].size;
            }
        },
        .arrow_right => {
            if (self.cy < self.rows.items.len) {
                const row_len = self.rows.items[self.cy].size;
                if (self.cx < row_len) {
                    self.cx += 1;
                } else if (self.cy + 1 < self.rows.items.len) {
                    self.cy += 1;
                    self.cx = 0;
                }
            }
        },
        .arrow_up => {
            if (self.cy > 0) self.cy -= 1;
        },
        .arrow_down => {
            if (self.cy + 1 < self.rows.items.len) self.cy += 1;
        },
        else => {},
    }

    clampCursor(self);
}

pub fn clampCursor(self: *Editor) void {
    if (self.cy < self.rows.items.len) {
        const row_len = self.rows.items[self.cy].size;
        if (self.cx > row_len) self.cx = row_len;
    } else {
        self.cx = 0;
    }
}

pub fn rowCxToRx(self: *const Editor, row_idx: usize, cx_val: usize) usize {
    if (row_idx >= self.rows.items.len) return cx_val;
    const row = self.rows.items[row_idx];
    const len = @min(cx_val, row.size);

    var line_buf: [4096]u8 = undefined;
    const read_len = @min(len, line_buf.len);
    self.text.copyRange(row.off, line_buf[0..read_len]);

    const tab_size = self.config.tab_size;
    var render_x: usize = 0;
    for (line_buf[0..read_len]) |c| {
        if (c == '\t') {
            render_x += (tab_size - 1) - (render_x % tab_size);
        }
        render_x += 1;
    }
    return render_x;
}

pub fn updateLineNumWidth(self: *Editor) void {
    if (!self.config.line_numbers) {
        self.line_num_width = 0;
        return;
    }
    var n = if (self.rows.items.len > 0) self.rows.items.len else 1;
    var digits: usize = 0;
    while (n > 0) {
        digits += 1;
        n /= 10;
    }
    if (digits < 2) digits = 2;
    self.line_num_width = digits + 1; // +1 for space after number
}
