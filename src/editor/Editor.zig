const std = @import("std");
const terminal = @import("../terminal.zig");
const PieceTable = @import("../piece_table.zig");
pub const config_mod = @import("config.zig");
pub const syntax_mod = @import("syntax.zig");
pub const Config = config_mod.Config;
pub const Color = config_mod.Color;
pub const SyntaxDef = syntax_mod.SyntaxDef;
pub const HlType = syntax_mod.HlType;

const Editor = @This();

// --- Sub-module method imports ---
const cursor = @import("cursor.zig");
const editing = @import("editing.zig");
const fileio = @import("fileio.zig");
const input = @import("input.zig");
const render = @import("render.zig");
const search = @import("search.zig");
const selection_mod = @import("selection.zig");
const command_log = @import("command_log.zig");

// Cursor
pub const scroll = cursor.scroll;
pub const moveCursor = cursor.moveCursor;
pub const rowCxToRx = cursor.rowCxToRx;
pub const clampCursor = cursor.clampCursor;
pub const updateLineNumWidth = cursor.updateLineNumWidth;

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
pub const switchFile = fileio.switchFile;

// Input
pub const processKeypress = input.processKeypress;
pub const prompt = input.prompt;

// Render
pub const refreshScreen = render.refreshScreen;

// Search
pub const find = search.find;
pub const findReplace = search.findReplace;
pub const gotoLine = search.gotoLine;

// Command log (undo/redo)
pub const recordInsert = command_log.recordInsert;
pub const recordDelete = command_log.recordDelete;
pub const breakSeq = command_log.breakSeq;
pub const undo = command_log.undo;
pub const redo = command_log.redo;

// Selection
pub const selectStart = selection_mod.selectStart;
pub const selectClear = selection_mod.selectClear;
pub const deleteSelection = selection_mod.deleteSelection;
pub const copy = selection_mod.copy;
pub const cut = selection_mod.cut;
pub const paste = selection_mod.paste;

// --- Types ---

pub const MouseEvent = struct {
    row: usize,
    col: usize,
};

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
    mouse_press: MouseEvent,
    mouse_drag: MouseEvent,
    mouse_release: MouseEvent,
    scroll_up,
    scroll_down,
};

pub const RowMeta = struct {
    off: usize,
    size: usize,
    hl_open_comment: bool = false,
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

copy_flash_start: ?i64 = null,
copy_flash_off: ?struct { start: usize, end: usize } = null,

undo_stack: std.ArrayList(command_log.Entry) = .empty,
redo_stack: std.ArrayList(command_log.Entry) = .empty,
cmd_seq: u64 = 0,
cmd_last_seq: u64 = 0,

quit_times: u8 = 2,

config: Config = .{},
syntax: ?*const SyntaxDef = null,
line_num_width: usize = 0,

// --- Core methods ---

pub fn init(allocator: std.mem.Allocator) !Editor {
    const screen = try terminal.getWindowSize();
    var sr: usize = @intCast(screen.rows);
    if (sr >= 2) sr -= 2; // reserve status bar + message bar

    const cfg = config_mod.load(allocator);

    var ed = Editor{
        .allocator = allocator,
        .text = try PieceTable.init(allocator, ""),
        .screen_rows = sr,
        .screen_cols = @intCast(screen.cols),
        .config = cfg,
    };
    try ed.rows.append(allocator, RowMeta{ .off = 0, .size = 0 });
    ed.updateLineNumWidth();
    return ed;
}

pub fn deinit(self: *Editor) void {
    self.text.deinit();
    if (self.base_buf) |buf| self.allocator.free(buf);
    if (self.filename) |name| self.allocator.free(name);
    if (self.clipboard) |clip| self.allocator.free(clip);
    command_log.deinitStacks(self);
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

pub fn textCols(self: *const Editor) usize {
    if (self.line_num_width >= self.screen_cols) return 0;
    return self.screen_cols - self.line_num_width;
}

pub fn updateSyntaxState(self: *Editor) void {
    syntax_mod.updateCommentState(self.syntax, self.rows.items, self.text);
}
