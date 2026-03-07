const terminal = @import("terminal.zig");
const std = @import("std");

pub fn main() !void {
    try terminal.enableRawMode();
    try terminal.clearScreen();

    const size = try terminal.getWindowSize();
    std.debug.print("{d}x{d}\n", .{ size.cols, size.rows });

    terminal.disableRawMode();
}
