const std = @import("std");
const terminal = @import("../terminal.zig");
const Editor = @import("Editor.zig");

fn ctrlKey(c: u8) u8 {
    return c & 0x1f;
}

/// Use poll() to check if stdin has data available within `timeout_ms`.
fn stdinHasData(timeout_ms: i32) bool {
    const POLLIN = 0x0001;
    var pfd = [1]std.posix.pollfd{.{
        .fd = std.posix.STDIN_FILENO,
        .events = POLLIN,
        .revents = 0,
    }};
    const n = std.posix.poll(&pfd, timeout_ms) catch return false;
    return n > 0 and (pfd[0].revents & POLLIN) != 0;
}

/// Drain any stale bytes sitting in stdin (e.g. from terminal responses).
pub fn drainInput() void {
    while (stdinHasData(0)) {
        var discard: [64]u8 = undefined;
        _ = std.posix.read(std.posix.STDIN_FILENO, &discard) catch break;
    }
}

fn readByte() !?u8 {
    var buf: [1]u8 = undefined;
    const n = try std.posix.read(std.posix.STDIN_FILENO, &buf);
    if (n == 0) return null;
    return buf[0];
}

pub fn readKey() !Editor.Key {
    // Block until a byte is available
    var buf: [1]u8 = undefined;
    while (true) {
        const n = try std.posix.read(std.posix.STDIN_FILENO, &buf);
        if (n == 0) continue;
        break;
    }
    const c = buf[0];

    if (c == 127) return Editor.Key{ .char = 127 }; // backspace
    if (c != '\x1b') return Editor.Key{ .char = c };

    // Got ESC byte — use poll to check if more bytes follow (escape sequence).
    // A bare ESC keypress won't have follow-up bytes.
    if (!stdinHasData(50)) return Editor.Key{ .char = '\x1b' };

    const s0 = (try readByte()) orelse return Editor.Key{ .char = '\x1b' };
    if (!stdinHasData(50)) return Editor.Key{ .char = '\x1b' };
    const s1 = (try readByte()) orelse return Editor.Key{ .char = '\x1b' };

    if (s0 == '[') {
        if (s1 >= '0' and s1 <= '9') {
            if (!stdinHasData(50)) return Editor.Key{ .char = '\x1b' };
            const s2 = (try readByte()) orelse return Editor.Key{ .char = '\x1b' };
            if (s2 == '~') {
                return switch (s1) {
                    '1' => Editor.Key.home,
                    '3' => Editor.Key.delete,
                    '4' => Editor.Key.end,
                    '5' => Editor.Key.page_up,
                    '6' => Editor.Key.page_down,
                    '7' => Editor.Key.home,
                    '8' => Editor.Key.end,
                    else => Editor.Key{ .char = '\x1b' },
                };
            }
            // Check for shift+arrow: ESC [ 1 ; 2 <dir>
            if (s1 == '1' and s2 == ';') {
                if (!stdinHasData(50)) return Editor.Key{ .char = '\x1b' };
                const mod = (try readByte()) orelse return Editor.Key{ .char = '\x1b' };
                if (!stdinHasData(50)) return Editor.Key{ .char = '\x1b' };
                const dir = (try readByte()) orelse return Editor.Key{ .char = '\x1b' };
                if (mod == '2') {
                    return switch (dir) {
                        'A' => Editor.Key.shift_arrow_up,
                        'B' => Editor.Key.shift_arrow_down,
                        'C' => Editor.Key.shift_arrow_right,
                        'D' => Editor.Key.shift_arrow_left,
                        else => Editor.Key{ .char = '\x1b' },
                    };
                }
            }
        }
        return switch (s1) {
            'A' => Editor.Key.arrow_up,
            'B' => Editor.Key.arrow_down,
            'C' => Editor.Key.arrow_right,
            'D' => Editor.Key.arrow_left,
            'H' => Editor.Key.home,
            'F' => Editor.Key.end,
            else => Editor.Key{ .char = '\x1b' },
        };
    } else if (s0 == 'O') {
        return switch (s1) {
            'H' => Editor.Key.home,
            'F' => Editor.Key.end,
            else => Editor.Key{ .char = '\x1b' },
        };
    }

    return Editor.Key{ .char = '\x1b' };
}

/// Returns false to signal quit.
pub fn processKeypress(self: *Editor) !bool {
    const key = try readKey();

    switch (key) {
        .char => |c| {
            if (c == ctrlKey('q')) {
                if (self.dirty > 0 and self.quit_times > 0) {
                    self.setStatusMessage("WARNING: Unsaved changes. Press Ctrl-Q {d} more time(s) to quit.", .{self.quit_times});
                    self.quit_times -= 1;
                    return true;
                }
                return false;
            } else if (c == ctrlKey('s')) {
                try self.save();
            } else if (c == ctrlKey('f')) {
                self.selectClear();
                try self.find();
            } else if (c == ctrlKey('r')) {
                self.selectClear();
                try self.findReplace();
            } else if (c == ctrlKey('g')) {
                self.selectClear();
                try self.gotoLine();
            } else if (c == ctrlKey('c')) {
                self.copy();
                self.selectClear();
            } else if (c == ctrlKey('x')) {
                try self.cut();
            } else if (c == ctrlKey('v')) {
                try self.paste();
            } else if (c == '\r') {
                if (self.selection != null) {
                    try self.deleteSelection();
                }
                try self.insertNewline();
            } else if (c == 127 or c == ctrlKey('h')) {
                // Backspace
                if (self.selection != null) {
                    try self.deleteSelection();
                } else {
                    try self.deleteChar();
                }
            } else if (c == '\t') {
                if (self.selection != null) try self.deleteSelection();
                if (self.config.expand_tabs) {
                    const spaces = self.config.tab_size - (self.cx % self.config.tab_size);
                    for (0..spaces) |_| try self.insertChar(' ');
                } else {
                    try self.insertChar('\t');
                }
            } else if (c == '\x1b' or c == ctrlKey('l')) {
                self.selectClear();
            } else if (c >= 32 and c < 127) {
                if (self.selection != null) try self.deleteSelection();
                try self.insertChar(c);
            }
        },

        .delete => {
            if (self.selection != null) {
                try self.deleteSelection();
            } else {
                try self.deleteForward();
            }
        },

        .shift_arrow_up => {
            self.selectStart();
            self.moveCursor(Editor.Key.arrow_up);
        },
        .shift_arrow_down => {
            self.selectStart();
            self.moveCursor(Editor.Key.arrow_down);
        },
        .shift_arrow_left => {
            self.selectStart();
            self.moveCursor(Editor.Key.arrow_left);
        },
        .shift_arrow_right => {
            self.selectStart();
            self.moveCursor(Editor.Key.arrow_right);
        },

        .arrow_up, .arrow_down, .arrow_left, .arrow_right => {
            self.selectClear();
            self.moveCursor(key);
        },

        .page_up => {
            self.selectClear();
            self.cy = self.row_offset;
            var t = self.screen_rows;
            while (t > 0) : (t -= 1) {
                self.moveCursor(Editor.Key.arrow_up);
            }
        },
        .page_down => {
            self.selectClear();
            self.cy = @min(self.row_offset + self.screen_rows - 1, if (self.rows.items.len > 0) self.rows.items.len - 1 else 0);
            var t = self.screen_rows;
            while (t > 0) : (t -= 1) {
                self.moveCursor(Editor.Key.arrow_down);
            }
        },
        .home => {
            self.selectClear();
            self.cx = 0;
        },
        .end => {
            self.selectClear();
            if (self.cy < self.rows.items.len) {
                self.cx = self.rows.items[self.cy].size;
            }
        },
    }

    self.quit_times = 2;
    return true;
}

/// Prompt the user for input in the message bar. Returns owned slice or null on ESC.
pub fn prompt(self: *Editor, comptime fmt: []const u8, args: anytype) !?[]u8 {
    _ = args;

    var buf = std.ArrayList(u8).empty;
    defer buf.deinit(self.allocator);

    while (true) {
        // Format status message with current input
        var msg_buf: [80]u8 = undefined;
        const msg = std.fmt.bufPrint(&msg_buf, fmt, .{buf.items}) catch buf.items;
        @memcpy(self.status_msg[0..msg.len], msg);
        self.status_msg_len = msg.len;
        self.status_time = std.time.timestamp();
        try self.refreshScreen();

        const key = try readKey();
        switch (key) {
            .char => |c| {
                if (c == '\x1b') {
                    self.setStatusMessage("", .{});
                    return null;
                } else if (c == '\r') {
                    if (buf.items.len > 0) {
                        self.setStatusMessage("", .{});
                        return try self.allocator.dupe(u8, buf.items);
                    }
                } else if (c == 127 or c == ctrlKey('h')) {
                    if (buf.items.len > 0) {
                        buf.items.len -= 1;
                    }
                } else if (c >= 32 and c < 127) {
                    try buf.append(self.allocator, c);
                }
            },
            .delete => {
                if (buf.items.len > 0) {
                    buf.items.len -= 1;
                }
            },
            else => {},
        }
    }
}
