const std = @import("std");

pub const Color = union(enum) {
    none,
    index: u8,
    rgb: RGB,

    pub const RGB = struct { r: u8, g: u8, b: u8 };

    pub fn writeFg(self: Color, buf: *std.ArrayList(u8), alloc: std.mem.Allocator) !void {
        var esc: [32]u8 = undefined;
        const s = switch (self) {
            .none => return,
            .index => |c| std.fmt.bufPrint(&esc, "\x1b[38;5;{d}m", .{c}) catch return,
            .rgb => |c| std.fmt.bufPrint(&esc, "\x1b[38;2;{d};{d};{d}m", .{ c.r, c.g, c.b }) catch return,
        };
        try buf.appendSlice(alloc, s);
    }

    pub fn writeBg(self: Color, buf: *std.ArrayList(u8), alloc: std.mem.Allocator) !void {
        var esc: [32]u8 = undefined;
        const s = switch (self) {
            .none => return,
            .index => |c| std.fmt.bufPrint(&esc, "\x1b[48;5;{d}m", .{c}) catch return,
            .rgb => |c| std.fmt.bufPrint(&esc, "\x1b[48;2;{d};{d};{d}m", .{ c.r, c.g, c.b }) catch return,
        };
        try buf.appendSlice(alloc, s);
    }

    pub fn eql(a: Color, b: Color) bool {
        const tag_a: u2 = switch (a) {
            .none => 0,
            .index => 1,
            .rgb => 2,
        };
        const tag_b: u2 = switch (b) {
            .none => 0,
            .index => 1,
            .rgb => 2,
        };
        if (tag_a != tag_b) return false;
        return switch (a) {
            .none => true,
            .index => |ia| ia == b.index,
            .rgb => |ra| ra.r == b.rgb.r and ra.g == b.rgb.g and ra.b == b.rgb.b,
        };
    }
};

fn tc(r: u8, g: u8, b: u8) Color {
    return .{ .rgb = .{ .r = r, .g = g, .b = b } };
}

pub const Config = struct {
    tab_size: usize = 4,
    expand_tabs: bool = false,
    line_numbers: bool = true,

    color_comment: Color = .{ .index = 6 },
    color_keyword1: Color = .{ .index = 3 },
    color_keyword2: Color = .{ .index = 2 },
    color_string: Color = .{ .index = 5 },
    color_number: Color = .{ .index = 1 },
    color_match: Color = .{ .index = 4 },
    color_bg: Color = .none,
    color_selection: Color = .{ .index = 240 },

    config_err: ?[]const u8 = null,
};

const ThemeDef = struct {
    name: []const u8,
    bg: Color,
    comment: Color,
    keyword1: Color,
    keyword2: Color,
    string: Color,
    number: Color,
    match: Color,
    selection: Color,
};

const builtin_themes = [_]ThemeDef{
    .{
        .name = "default",
        .bg = .none,
        .comment = .{ .index = 6 },
        .keyword1 = .{ .index = 3 },
        .keyword2 = .{ .index = 2 },
        .string = .{ .index = 5 },
        .number = .{ .index = 1 },
        .match = .{ .index = 4 },
        .selection = .{ .index = 240 },
    },
    .{
        .name = "monokai",
        .bg = tc(0x27, 0x28, 0x22),
        .comment = tc(0x75, 0x71, 0x5e),
        .keyword1 = tc(0xf9, 0x26, 0x72),
        .keyword2 = tc(0x66, 0xd9, 0xef),
        .string = tc(0xe6, 0xdb, 0x74),
        .number = tc(0xae, 0x81, 0xff),
        .match = tc(0xe6, 0xdb, 0x74),
        .selection = tc(0x49, 0x48, 0x3e),
    },
    .{
        .name = "dracula",
        .bg = tc(0x28, 0x2a, 0x36),
        .comment = tc(0x62, 0x72, 0xa4),
        .keyword1 = tc(0xff, 0x79, 0xc6),
        .keyword2 = tc(0x50, 0xfa, 0x7b),
        .string = tc(0xf1, 0xfa, 0x8c),
        .number = tc(0xbd, 0x93, 0xf9),
        .match = tc(0xf1, 0xfa, 0x8c),
        .selection = tc(0x44, 0x47, 0x5a),
    },
    .{
        .name = "nord",
        .bg = tc(0x2e, 0x34, 0x40),
        .comment = tc(0x4c, 0x56, 0x6a),
        .keyword1 = tc(0x81, 0xa1, 0xc1),
        .keyword2 = tc(0x88, 0xc0, 0xd0),
        .string = tc(0xa3, 0xbe, 0x8c),
        .number = tc(0xd0, 0x87, 0x70),
        .match = tc(0xeb, 0xcb, 0x8b),
        .selection = tc(0x43, 0x4c, 0x5e),
    },
    .{
        .name = "gruvbox",
        .bg = tc(0x28, 0x28, 0x28),
        .comment = tc(0x92, 0x83, 0x74),
        .keyword1 = tc(0xfb, 0x49, 0x34),
        .keyword2 = tc(0xfa, 0xbd, 0x2f),
        .string = tc(0xb8, 0xbb, 0x26),
        .number = tc(0xd3, 0x86, 0x9b),
        .match = tc(0xeb, 0xcb, 0x8b),
        .selection = tc(0x50, 0x49, 0x45),
    },
    .{
        .name = "solarized-dark",
        .bg = tc(0x00, 0x2b, 0x36),
        .comment = tc(0x58, 0x6e, 0x75),
        .keyword1 = tc(0x26, 0x8b, 0xd2),
        .keyword2 = tc(0x2a, 0xa1, 0x98),
        .string = tc(0x85, 0x99, 0x00),
        .number = tc(0xcb, 0x4b, 0x16),
        .match = tc(0xb5, 0x89, 0x00),
        .selection = tc(0x07, 0x36, 0x42),
    },
    .{
        .name = "one-dark",
        .bg = tc(0x28, 0x2c, 0x34),
        .comment = tc(0x5c, 0x63, 0x70),
        .keyword1 = tc(0xc6, 0x78, 0xdd),
        .keyword2 = tc(0x61, 0xaf, 0xef),
        .string = tc(0xd1, 0x9a, 0x66),
        .number = tc(0xe0, 0x6c, 0x75),
        .match = tc(0xe5, 0xc0, 0x7b),
        .selection = tc(0x3e, 0x44, 0x51),
    },
    .{
        .name = "retrobox",
        .bg = tc(0x1d, 0x20, 0x21),
        .comment = tc(0x66, 0x5c, 0x54),
        .keyword1 = tc(0xfe, 0x80, 0x19),
        .keyword2 = tc(0xfa, 0xbd, 0x2f),
        .string = tc(0xb8, 0xbb, 0x26),
        .number = tc(0xd3, 0x86, 0x9b),
        .match = tc(0xfa, 0xbd, 0x2f),
        .selection = tc(0x3c, 0x38, 0x36),
    },
};

fn applyTheme(cfg: *Config, theme: *const ThemeDef) void {
    cfg.color_bg = theme.bg;
    cfg.color_comment = theme.comment;
    cfg.color_keyword1 = theme.keyword1;
    cfg.color_keyword2 = theme.keyword2;
    cfg.color_string = theme.string;
    cfg.color_number = theme.number;
    cfg.color_match = theme.match;
    cfg.color_selection = theme.selection;
}

fn parseColor(val: []const u8) ?Color {
    if (val.len == 0) return null;
    if (val.len == 7 and val[0] == '#') {
        const r = std.fmt.parseInt(u8, val[1..3], 16) catch return null;
        const g = std.fmt.parseInt(u8, val[3..5], 16) catch return null;
        const b = std.fmt.parseInt(u8, val[5..7], 16) catch return null;
        return tc(r, g, b);
    }
    if (std.mem.eql(u8, val, "-1")) return Color.none;
    const n = std.fmt.parseInt(u8, val, 10) catch return null;
    return .{ .index = n };
}

fn applyColorKv(cfg: *Config, key: []const u8, val: []const u8) bool {
    const color = parseColor(val) orelse return false;
    if (std.mem.eql(u8, key, "color_bg")) {
        cfg.color_bg = color;
    } else if (std.mem.eql(u8, key, "color_comment")) {
        cfg.color_comment = color;
    } else if (std.mem.eql(u8, key, "color_keyword1")) {
        cfg.color_keyword1 = color;
    } else if (std.mem.eql(u8, key, "color_keyword2")) {
        cfg.color_keyword2 = color;
    } else if (std.mem.eql(u8, key, "color_string")) {
        cfg.color_string = color;
    } else if (std.mem.eql(u8, key, "color_number")) {
        cfg.color_number = color;
    } else if (std.mem.eql(u8, key, "color_match")) {
        cfg.color_match = color;
    } else if (std.mem.eql(u8, key, "color_selection")) {
        cfg.color_selection = color;
    } else {
        return false;
    }
    return true;
}

fn isColorKey(key: []const u8) bool {
    const color_keys = [_][]const u8{
        "color_bg",       "color_comment",  "color_keyword1", "color_keyword2",
        "color_string",   "color_number",   "color_match",    "color_selection",
    };
    for (&color_keys) |k| {
        if (std.mem.eql(u8, key, k)) return true;
    }
    return false;
}

fn ltrim(s: []const u8) []const u8 {
    var i: usize = 0;
    while (i < s.len and (s[i] == ' ' or s[i] == '\t')) : (i += 1) {}
    return s[i..];
}

fn rtrim(s: []const u8) []const u8 {
    var e = s.len;
    while (e > 0 and (s[e - 1] == ' ' or s[e - 1] == '\t' or s[e - 1] == '\r' or s[e - 1] == '\n')) : (e -= 1) {}
    return s[0..e];
}

fn trim(s: []const u8) []const u8 {
    return rtrim(ltrim(s));
}

fn stripComment(line: []const u8) []const u8 {
    // Find '=' to protect color values like #RRGGBB
    const eq_pos = std.mem.indexOfScalar(u8, line, '=');
    if (eq_pos) |ep| {
        // Only strip comments before '='
        for (line[0..ep]) |c| {
            if (c == '#') return line[0..0];
        }
        // After '=', find the value — skip first '#' if it starts with '#' (color)
        const after_eq = ltrim(line[ep + 1 ..]);
        var scan_start: usize = ep + 1 + (line.len - ep - 1 - after_eq.len);
        if (after_eq.len > 0 and after_eq[0] == '#') {
            scan_start += 1; // skip color '#'
            // Find second '#' as comment
            if (scan_start < line.len) {
                if (std.mem.indexOfScalarPos(u8, line, scan_start, '#')) |hp| {
                    return line[0..hp];
                }
            }
        } else {
            if (std.mem.indexOfScalarPos(u8, line, scan_start, '#')) |hp| {
                return line[0..hp];
            }
        }
        return line;
    }
    // No '=', strip from first '#'
    if (std.mem.indexOfScalar(u8, line, '#')) |hp| {
        return line[0..hp];
    }
    return line;
}

fn parseKvLine(line: []const u8) ?struct { key: []const u8, val: []const u8 } {
    const stripped = stripComment(line);
    const s = trim(stripped);
    if (s.len == 0) return null;
    const eq_pos = std.mem.indexOfScalar(u8, s, '=') orelse return null;
    const key = trim(s[0..eq_pos]);
    const val = trim(s[eq_pos + 1 ..]);
    if (key.len == 0 or val.len == 0) return null;
    return .{ .key = key, .val = val };
}

fn loadThemeFile(cfg: *Config, name: []const u8, home: []const u8, allocator: std.mem.Allocator) bool {
    var path_buf: [1024]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buf, "{s}/.eko/themes/{s}.theme", .{ home, name }) catch return false;

    const file = std.fs.cwd().openFile(path, .{}) catch return false;
    defer file.close();

    const content = file.readToEndAlloc(allocator, 64 * 1024) catch return false;
    defer allocator.free(content);

    var it = std.mem.splitScalar(u8, content, '\n');
    while (it.next()) |line| {
        const kv = parseKvLine(line) orelse continue;
        if (isColorKey(kv.key)) {
            _ = applyColorKv(cfg, kv.key, kv.val);
        }
    }
    return true;
}

fn applyThemeByName(cfg: *Config, name: []const u8, home: []const u8, allocator: std.mem.Allocator) bool {
    // Try user theme file first
    if (loadThemeFile(cfg, name, home, allocator)) return true;
    // Try builtin themes
    for (&builtin_themes) |*theme| {
        if (std.mem.eql(u8, theme.name, name)) {
            applyTheme(cfg, theme);
            return true;
        }
    }
    return false;
}

pub fn load(allocator: std.mem.Allocator) Config {
    var cfg = Config{};

    const home = std.posix.getenv("HOME") orelse return cfg;

    var path_buf: [1024]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buf, "{s}/.ekorc", .{home}) catch return cfg;

    const file = std.fs.cwd().openFile(path, .{}) catch return cfg;
    defer file.close();

    const content = file.readToEndAlloc(allocator, 64 * 1024) catch return cfg;
    defer allocator.free(content);

    // First pass: find theme
    var it = std.mem.splitScalar(u8, content, '\n');
    while (it.next()) |line| {
        const kv = parseKvLine(line) orelse continue;
        if (std.mem.eql(u8, kv.key, "theme")) {
            _ = applyThemeByName(&cfg, kv.val, home, allocator);
            break;
        }
    }

    // Second pass: apply all settings (overrides theme colors)
    it = std.mem.splitScalar(u8, content, '\n');
    while (it.next()) |line| {
        const kv = parseKvLine(line) orelse continue;

        if (std.mem.eql(u8, kv.key, "theme")) {
            continue; // already handled
        } else if (std.mem.eql(u8, kv.key, "tab_size")) {
            const n = std.fmt.parseInt(usize, kv.val, 10) catch continue;
            if (n >= 1 and n <= 16) cfg.tab_size = n;
        } else if (std.mem.eql(u8, kv.key, "expand_tabs")) {
            if (std.mem.eql(u8, kv.val, "true")) {
                cfg.expand_tabs = true;
            } else if (std.mem.eql(u8, kv.val, "false")) {
                cfg.expand_tabs = false;
            }
        } else if (std.mem.eql(u8, kv.key, "line_numbers")) {
            if (std.mem.eql(u8, kv.val, "true")) {
                cfg.line_numbers = true;
            } else if (std.mem.eql(u8, kv.val, "false")) {
                cfg.line_numbers = false;
            }
        } else if (isColorKey(kv.key)) {
            _ = applyColorKv(&cfg, kv.key, kv.val);
        }
    }

    return cfg;
}
