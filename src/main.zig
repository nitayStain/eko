const terminal = @import("terminal.zig");
const std = @import("std");

pub fn main() !void {
    try terminal.enableRawMode();
    try terminal.clearScreen();
    terminal.disableRawMode();
}
