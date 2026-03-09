const std = @import("std");
const Editor = @import("Editor.zig");

pub fn insertChar(self: *Editor, c: u8) !void {
    if (self.cy >= self.rows.items.len) return;

    const off = self.rows.items[self.cy].off + self.cx;
    self.recordInsert(off, &[_]u8{c});
    try self.text.insert(off, &[_]u8{c});

    self.rows.items[self.cy].size += 1;
    shiftOffsetsFrom(self, self.cy + 1, 1);
    self.cx += 1;
    self.dirty += 1;
    self.markDirtyRow(self.cy);
}

pub fn insertNewline(self: *Editor) !void {
    if (self.cy >= self.rows.items.len) return;

    const old_size = self.rows.items[self.cy].size;
    const split_off = self.rows.items[self.cy].off + self.cx;

    self.breakSeq();
    self.recordInsert(split_off, "\n");
    try self.text.insert(split_off, "\n");

    self.rows.items[self.cy].size = self.cx;

    const new_off = split_off + 1;
    const new_size = old_size - self.cx;
    try self.rows.insert(self.allocator, self.cy + 1, Editor.RowMeta{ .off = new_off, .size = new_size });
    shiftOffsetsFrom(self, self.cy + 2, 1);

    self.cy += 1;
    self.cx = 0;
    self.dirty += 1;
    self.markDirtyFrom(self.cy - 1);
}

pub fn deleteChar(self: *Editor) !void {
    if (self.cy >= self.rows.items.len) return;
    if (self.cx == 0 and self.cy == 0) return;

    if (self.cx > 0) {
        const off = self.rows.items[self.cy].off + self.cx - 1;
        var del: [1]u8 = undefined;
        self.text.copyRange(off, &del);
        self.recordDelete(off, &del);
        try self.text.delete(off, 1);
        self.rows.items[self.cy].size -= 1;
        shiftOffsetsFrom(self, self.cy + 1, -1);
        self.cx -= 1;
        self.dirty += 1;
        self.markDirtyRow(self.cy);
    } else {
        const prev_size = self.rows.items[self.cy - 1].size;
        const nl_off = self.rows.items[self.cy - 1].off + prev_size;

        self.breakSeq();
        self.recordDelete(nl_off, "\n");
        try self.text.delete(nl_off, 1);

        self.rows.items[self.cy - 1].size += self.rows.items[self.cy].size;

        // Remove current row
        _ = self.rows.orderedRemove(self.cy);
        shiftOffsetsFrom(self, self.cy, -1);

        self.cy -= 1;
        self.cx = prev_size;
        self.dirty += 1;
        self.markDirtyFrom(self.cy);
    }
}

pub fn deleteForward(self: *Editor) !void {
    if (self.cy >= self.rows.items.len) return;

    if (self.cx < self.rows.items[self.cy].size) {
        const off = self.rows.items[self.cy].off + self.cx;
        var del: [1]u8 = undefined;
        self.text.copyRange(off, &del);
        self.recordDelete(off, &del);
        try self.text.delete(off, 1);
        self.rows.items[self.cy].size -= 1;
        shiftOffsetsFrom(self, self.cy + 1, -1);
        self.dirty += 1;
        self.markDirtyRow(self.cy);
    } else if (self.cy + 1 < self.rows.items.len) {
        const nl_off = self.rows.items[self.cy].off + self.rows.items[self.cy].size;
        self.breakSeq();
        self.recordDelete(nl_off, "\n");
        try self.text.delete(nl_off, 1);

        self.rows.items[self.cy].size += self.rows.items[self.cy + 1].size;
        _ = self.rows.orderedRemove(self.cy + 1);
        shiftOffsetsFrom(self, self.cy + 1, -1);
        self.dirty += 1;
        self.markDirtyFrom(self.cy);
    }
}

pub fn shiftOffsetsFrom(self: *Editor, from: usize, delta: isize) void {
    if (from >= self.rows.items.len) return;
    for (self.rows.items[from..]) |*row| {
        if (delta >= 0) {
            row.off += @intCast(delta);
        } else {
            row.off -= @intCast(@abs(delta));
        }
    }
}

pub fn rebuildRows(self: *Editor) !void {
    self.markDirtyAll();
    self.rows.clearRetainingCapacity();

    const total_len = self.text.getTotalLength();
    if (total_len == 0) {
        try self.rows.append(self.allocator, Editor.RowMeta{ .off = 0, .size = 0 });
        return;
    }

    var scan_buf: [4096]u8 = undefined;
    var pos: usize = 0;
    var line_start: usize = 0;

    while (pos < total_len) {
        const chunk_len = @min(scan_buf.len, total_len - pos);
        self.text.copyRange(pos, scan_buf[0..chunk_len]);
        for (scan_buf[0..chunk_len], 0..) |c, j| {
            if (c == '\n') {
                try self.rows.append(self.allocator, Editor.RowMeta{ .off = line_start, .size = (pos + j) - line_start });
                line_start = pos + j + 1;
            }
        }
        pos += chunk_len;
    }
    try self.rows.append(self.allocator, Editor.RowMeta{ .off = line_start, .size = total_len - line_start });
}
