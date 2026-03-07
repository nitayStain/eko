const std = @import("std");
const posix = std.posix;

var default_termios: posix.termios = undefined;

pub fn enableRawMode() !void {
    default_termios = try posix.tcgetattr(posix.STDIN_FILENO);

    var raw = default_termios;

    raw.iflag.BRKINT = false;
    raw.iflag.INPCK = false;
    raw.iflag.ISTRIP = false;
    raw.iflag.ICRNL = false;
    raw.iflag.IXON = false;

    raw.oflag.OPOST = false;

    raw.cflag.CSIZE = .CS8;

    raw.lflag.ECHO = false;
    raw.lflag.ICANON = false;
    raw.lflag.ISIG = false;
    raw.lflag.IEXTEN = false;

    raw.cc[@intFromEnum(posix.V.MIN)] = 0;
    raw.cc[@intFromEnum(posix.V.TIME)] = 1;

    try posix.tcsetattr(posix.STDIN_FILENO, .FLUSH, raw);
}

pub fn disableRawMode() void {
    posix.tcsetattr(posix.STDIN_FILENO, .FLUSH, default_termios) catch {};
}

pub fn clearScreen() !void {
    const stdout = std.fs.File.stdout();
    try stdout.writeAll("\x1b[2J\x1b[H");
}
