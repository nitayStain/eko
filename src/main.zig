const std = @import("std");
const terminal = @import("terminal.zig");
const Editor = @import("editor/Editor.zig");

pub fn main() !void {
    try terminal.enableRawMode();
    defer terminal.disableRawMode();

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ed = try Editor.init(allocator);
    defer ed.deinit();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len >= 2) {
        try ed.open(args[1]);
    }

    if (ed.config.config_err) |err| {
        ed.setStatusMessage("Config error: {s}", .{err});
    } else {
        ed.setStatusMessage("^S=save ^F=find ^R=replace ^G=goto ^Q=quit", .{});
    }

    while (true) {
        try ed.refreshScreen();
        if (!try ed.processKeypress()) break;
    }

    try terminal.write("\x1b[2J\x1b[H");
}
