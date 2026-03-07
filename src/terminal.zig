const std = @import("std");
const posix = std.posix;

pub const Size = struct { rows: u16, cols: u16 };

var default_termios: posix.termios = undefined;

pub fn write(msg: []const u8) !void {
    const stdout = std.fs.File.stdout();
    try stdout.writeAll(msg);
}

pub fn clearScreen() !void {
    write("\x1b[2J\x1b[H") catch {};
}

pub fn die(msg: []const u8) noreturn {
    clearScreen() catch {};
    write(msg) catch {};
    std.process.exit(1);
}

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

pub fn getCursorPosition() !Size {
    var buf: [32]u8 = undefined;
    var i: u32 = 0;

    try write("\x1b[6n");

    while (i < buf.len) {
        const n = try std.posix.read(std.posix.STDIN_FILENO, buf[i .. i + 1]);
        if (n == 0) return error.EndOfStream;
        if (buf[i] == 'R') break;
        i += 1;
    }

    if (buf[0] != '\x1b' or buf[1] != '[') return error.InvalidPosition;
    const payload = buf[2..i];

    var it = std.mem.splitScalar(u8, payload, ';');
    const rows = try std.fmt.parseInt(u16, it.next() orelse return error.InvalidPosition, 10);
    const cols = try std.fmt.parseInt(u16, it.next() orelse return error.InvalidPosition, 10);

    return Size{ .rows = rows, .cols = cols };
}

pub fn getWindowSize() !Size {
    var winsize: posix.winsize = undefined;

    const err = posix.system.ioctl(posix.STDOUT_FILENO, posix.T.IOCGWINSZ, @intFromPtr(&winsize));
    if (posix.errno(err) != .SUCCESS) {
        return error.PosixFailure;
    }

    return Size{ .rows = winsize.row, .cols = winsize.col };
}
