const std = @import("std");
const terminal = @import("../terminal.zig");
const PieceTable = @import("../piece_table.zig");

const Editor = @This();

// --- Sub-module method imports ---
const cursor = @import("cursor.zig");
const editing = @import("editing.zig");
const fileio = @import("fileio.zig");
const input = @import("input.zig");
const render = @import("render.zig");
const search = @import("search.zig");
const selection_mod = @import("selection.zig");

// Cursor
pub const scroll = cursor.scroll;
pub const moveCursor = cursor.moveCursor;
pub const rowCxToRx = cursor.rowCxToRx;
pub const clampCursor = cursor.clampCursor;

// Editing
pub const insertChar = editing.insertChar;
pub const insertNewline = editing.insertNewline;
pub const deleteChar = editing.deleteChar;
pub const deleteForward = editing.deleteForward;
pub const shiftOffsetsFrom = editing.shiftOffsetsFrom;
pub const rebuildRows = editing.rebuildRows;

// File I/O
pub const open = fileio.open;
pub const save = fileio.save;

// Input
pub const processKeypress = input.processKeypress;
pub const prompt = input.prompt;

// Render
pub const refreshScreen = render.refreshScreen;

// Search
pub const find = search.find;
pub const findReplace = search.findReplace;
pub const gotoLine = search.gotoLine;

// Selection
pub const selectStart = selection_mod.selectStart;
pub const selectClear = selection_mod.selectClear;
pub const deleteSelection = selection_mod.deleteSelection;
pub const copy = selection_mod.copy;
pub const cut = selection_mod.cut;
pub const paste = selection_mod.paste;

// --- Types ---

pub const Key = union(enum) {
    char: u8,
    arrow_up,
    arrow_down,
    arrow_left,
    arrow_right,
    shift_arrow_up,
    shift_arrow_down,
    shift_arrow_left,
    shift_arrow_right,
    page_up,
    page_down,
    home,
    end,
    delete,
    none,
};

pub const RowMeta = struct {
    off: usize,
    size: usize,
};

pub const Selection = struct {
    mark_x: usize,
    mark_y: usize,
};

// --- Fields ---

allocator: std.mem.Allocator,
text: PieceTable,
base_buf: ?[]u8 = null,
rows: std.ArrayList(RowMeta) = .empty,
render_buf: std.ArrayList(u8) = .empty,
cx: usize = 0,
cy: usize = 0,
rx: usize = 0,
row_offset: usize = 0,
col_offset: usize = 0,
screen_rows: usize,
screen_cols: usize,
filename: ?[]u8 = null,
dirty: usize = 0,

status_msg: [80]u8 = [_]u8{0} ** 80,
status_msg_len: usize = 0,
status_time: i64 = 0,

selection: ?Selection = null,
clipboard: ?[]u8 = null,

quit_times: u8 = 2,
tab_size: usize = 4,

// --- Core methods ---

pub fn init(allocator: std.mem.Allocator) !Editor {
    const screen = try terminal.getWindowSize();
    var sr: usize = @intCast(screen.rows);
    if (sr >= 2) sr -= 2; // reserve status bar + message bar
    var ed = Editor{
        .allocator = allocator,
        .text = try PieceTable.init(allocator, ""),
        .screen_rows = sr,
        .screen_cols = @intCast(screen.cols),
    };
    try ed.rows.append(allocator, RowMeta{ .off = 0, .size = 0 });
    return ed;
}

pub fn deinit(self: *Editor) void {
    self.text.deinit();
    if (self.base_buf) |buf| self.allocator.free(buf);
    if (self.filename) |name| self.allocator.free(name);
    if (self.clipboard) |clip| self.allocator.free(clip);
    self.rows.deinit(self.allocator);
    self.render_buf.deinit(self.allocator);
}

pub fn cursorOffset(self: *const Editor) usize {
    if (self.cy < self.rows.items.len) {
        return self.rows.items[self.cy].off + self.cx;
    }
    return self.text.getTotalLength();
}

pub fn setCursorFromOffset(self: *Editor, off: usize) void {
    var pos: usize = 0;
    for (self.rows.items, 0..) |row, i| {
        if (off >= pos and off <= pos + row.size) {
            self.cy = i;
            self.cx = off - pos;
            return;
        }
        pos += row.size + 1;
    }
    if (self.rows.items.len > 0) {
        self.cy = self.rows.items.len - 1;
        self.cx = self.rows.items[self.rows.items.len - 1].size;
    } else {
        self.cy = 0;
        self.cx = 0;
    }
}

pub fn setStatusMessage(self: *Editor, comptime fmt: []const u8, args: anytype) void {
    const result = std.fmt.bufPrint(&self.status_msg, fmt, args) catch {
        self.status_msg_len = 0;
        return;
    };
    self.status_msg_len = result.len;
    self.status_time = std.time.timestamp();
}

pub fn textRows(self: *const Editor) usize {
    return self.screen_rows;
}
