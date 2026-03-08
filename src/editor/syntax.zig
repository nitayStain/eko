const std = @import("std");
const config_mod = @import("config.zig");
const Color = config_mod.Color;
const Config = config_mod.Config;

pub const HlType = enum(u8) {
    normal = 0,
    comment,
    mlcomment,
    keyword1,
    keyword2,
    string,
    number,
    match,
};

pub const SyntaxDef = struct {
    filetype: []const u8,
    extensions: []const []const u8,
    keywords1: []const []const u8,
    keywords2: []const []const u8,
    singleline_comment: ?[]const u8 = null,
    multiline_comment_start: ?[]const u8 = null,
    multiline_comment_end: ?[]const u8 = null,
    highlight_numbers: bool = false,
    highlight_strings: bool = false,
};

fn isSeparator(c: u8) bool {
    if (c == 0) return true;
    if (std.ascii.isWhitespace(c)) return true;
    return std.mem.indexOfScalar(u8, ",.()+-/*=~%<>[];{}|&^!\"'`?:\\@#", c) != null;
}

// --- Built-in language databases ---

const c_kw1 = &[_][]const u8{
    "auto",     "break",     "case",    "continue", "default",   "do",       "else",
    "enum",     "extern",    "for",     "goto",     "if",        "inline",   "register",
    "restrict", "return",    "sizeof",  "static",   "struct",    "switch",   "typedef",
    "union",    "volatile",  "while",   "class",    "namespace", "template", "new",
    "delete",   "try",       "catch",   "throw",    "override",  "final",    "public",
    "private",  "protected", "virtual", "explicit", "friend",    "operator", "using",
    "#include", "#define",   "#undef",  "#ifdef",   "#ifndef",   "#if",      "#elif",
    "#else",    "#endif",    "#pragma", "#error",
};
const c_kw2 = &[_][]const u8{
    "char",     "double",   "float",  "int",     "long",    "short",   "unsigned",
    "signed",   "void",     "bool",   "size_t",  "ssize_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "NULL",
    "nullptr",  "true",     "false",
};

const py_kw1 = &[_][]const u8{
    "False",  "None",   "True",    "and",      "as",       "assert", "async",
    "await",  "break",  "class",   "continue", "def",      "del",    "elif",
    "else",   "except", "finally", "for",      "from",     "global", "if",
    "import", "in",     "is",      "lambda",   "nonlocal", "not",    "or",
    "pass",   "raise",  "return",  "try",      "while",    "with",   "yield",
};
const py_kw2 = &[_][]const u8{
    "int",      "str",         "float",        "bool", "list",   "dict",  "set",
    "tuple",    "bytes",       "bytearray",    "type", "object", "print", "len",
    "range",    "enumerate",   "zip",          "map",  "filter", "open",  "super",
    "property", "classmethod", "staticmethod",
};

const js_kw1 = &[_][]const u8{
    "break",  "case",   "catch",  "continue",   "debugger", "default", "delete",
    "do",     "else",   "export", "extends",    "finally",  "for",     "function",
    "if",     "import", "in",     "instanceof", "new",      "return",  "super",
    "switch", "this",   "throw",  "try",        "typeof",   "var",     "void",
    "while",  "with",   "yield",  "let",        "const",    "async",   "await",
    "class",  "of",     "from",   "static",     "get",      "set",
};
const js_kw2 = &[_][]const u8{
    "string",    "number",  "boolean", "object", "null",      "undefined", "void",
    "never",     "any",     "unknown", "symbol", "bigint",    "true",      "false",
    "Array",     "Promise", "Map",     "Set",    "interface", "type",      "enum",
    "namespace", "declare",
};

const rs_kw1 = &[_][]const u8{
    "as",     "async", "await", "break",  "const",  "continue", "crate",
    "dyn",    "else",  "enum",  "extern", "fn",     "for",      "if",
    "impl",   "in",    "let",   "loop",   "match",  "mod",      "move",
    "mut",    "pub",   "ref",   "return", "self",   "Self",     "static",
    "struct", "super", "trait", "type",   "unsafe", "use",      "where",
    "while",
};
const rs_kw2 = &[_][]const u8{
    "i8",   "i16",  "i32", "i64",    "i128",  "isize",  "u8",
    "u16",  "u32",  "u64", "u128",   "usize", "f32",    "f64",
    "bool", "char", "str", "String", "Vec",   "Option", "Result",
    "Box",  "Rc",   "Arc", "true",   "false", "None",   "Some",
    "Ok",   "Err",
};

const go_kw1 = &[_][]const u8{
    "break",  "case",        "chan", "const",   "continue", "default", "defer",
    "else",   "fallthrough", "for",  "func",    "go",       "goto",    "if",
    "import", "interface",   "map",  "package", "range",    "return",  "select",
    "struct", "switch",      "type", "var",
};
const go_kw2 = &[_][]const u8{
    "bool",   "byte",  "complex64", "complex128", "error",  "float32", "float64",
    "int",    "int8",  "int16",     "int32",      "int64",  "rune",    "string",
    "uint",   "uint8", "uint16",    "uint32",     "uint64", "uintptr", "nil",
    "true",   "false", "iota",      "make",       "new",    "len",     "cap",
    "append", "copy",  "close",     "delete",     "panic",  "recover",
};

const sh_kw1 = &[_][]const u8{
    "if",     "then",  "else",  "elif",     "fi",    "for",      "in",
    "do",     "done",  "while", "until",    "case",  "esac",     "function",
    "return", "exit",  "break", "continue", "local", "readonly", "export",
    "unset",  "shift", "set",
};
const sh_kw2 = &[_][]const u8{
    "echo", "printf", "read", "cd",   "pwd",  "ls",  "mkdir",
    "rm",   "mv",     "cp",   "cat",  "grep", "sed", "awk",
    "cut",  "source", "eval", "exec", "test",
};

const json_kw2 = &[_][]const u8{ "true", "false", "null" };

const yaml_kw2 = &[_][]const u8{
    "true", "false", "null", "yes", "no", "on", "off",
    "True", "False", "Null", "Yes", "No", "On", "Off",
};

const toml_kw2 = &[_][]const u8{ "true", "false" };

const zig_kw1 = &[_][]const u8{
    "align",     "and",         "asm",      "break",   "catch",
    "comptime",  "const",       "continue", "defer",   "else",
    "enum",      "errdefer",    "error",    "export",  "extern",
    "fn",        "for",         "if",       "inline",  "noalias",
    "nosuspend", "or",          "orelse",   "packed",  "pub",
    "resume",    "return",      "struct",   "suspend", "switch",
    "test",      "threadlocal", "try",      "union",   "unreachable",
    "var",       "volatile",    "while",
};
const zig_kw2 = &[_][]const u8{
    "anyerror", "anyframe",       "anyopaque",    "anytype",
    "bool",     "comptime_float", "comptime_int", "f16",
    "f32",      "f64",            "f80",          "f128",
    "i8",       "i16",            "i32",          "i64",
    "i128",     "isize",          "noreturn",     "null",
    "type",     "undefined",      "u8",           "u16",
    "u32",      "u64",            "u128",         "usize",
    "void",     "true",           "false",
};

const sql_kw1 = &[_][]const u8{
    "SELECT",     "FROM",      "WHERE",    "AND",         "OR",          "NOT",       "IN",
    "IS",         "LIKE",      "EXISTS",   "ORDER",       "BY",          "GROUP",     "HAVING",
    "LIMIT",      "OFFSET",    "DISTINCT", "ALL",         "JOIN",        "INNER",     "LEFT",
    "RIGHT",      "FULL",      "OUTER",    "CROSS",       "ON",          "USING",     "AS",
    "UNION",      "INTERSECT", "EXCEPT",   "INSERT",      "INTO",        "VALUES",    "UPDATE",
    "SET",        "DELETE",    "CREATE",   "TABLE",       "INDEX",       "VIEW",      "DATABASE",
    "SCHEMA",     "DROP",      "ALTER",    "ADD",         "COLUMN",      "RENAME",    "TRUNCATE",
    "CONSTRAINT", "PRIMARY",   "KEY",      "FOREIGN",     "REFERENCES",  "UNIQUE",    "DEFAULT",
    "CHECK",      "BEGIN",     "COMMIT",   "ROLLBACK",    "TRANSACTION", "SAVEPOINT", "CASE",
    "WHEN",       "THEN",      "ELSE",     "END",         "BETWEEN",     "CAST",      "COALESCE",
    "select",     "from",      "where",    "and",         "or",          "not",       "in",
    "is",         "like",      "exists",   "order",       "by",          "group",     "having",
    "limit",      "offset",    "distinct", "all",         "join",        "inner",     "left",
    "right",      "full",      "outer",    "cross",       "on",          "using",     "as",
    "union",      "intersect", "except",   "insert",      "into",        "values",    "update",
    "set",        "delete",    "create",   "table",       "index",       "view",      "database",
    "schema",     "drop",      "alter",    "add",         "column",      "rename",    "truncate",
    "begin",      "commit",    "rollback", "transaction", "savepoint",   "case",      "when",
    "then",       "else",      "end",      "between",     "cast",        "coalesce",
};
const sql_kw2 = &[_][]const u8{
    "INT",     "INTEGER", "BIGINT",  "SMALLINT", "SERIAL",    "FLOAT",    "DOUBLE",
    "DECIMAL", "NUMERIC", "REAL",    "VARCHAR",  "CHAR",      "TEXT",     "BLOB",
    "JSON",    "JSONB",   "DATE",    "DATETIME", "TIMESTAMP", "TIME",     "BOOLEAN",
    "BOOL",    "BYTEA",   "int",     "integer",  "bigint",    "smallint", "serial",
    "float",   "double",  "decimal", "numeric",  "real",      "varchar",  "char",
    "text",    "blob",    "json",    "jsonb",    "date",      "datetime", "timestamp",
    "time",    "boolean", "bool",
};

const css_kw1 = &[_][]const u8{
    "color",          "background",      "border",          "margin",    "padding",     "display",
    "width",          "height",          "position",        "font-size", "font-family", "font-weight",
    "text-align",     "text-decoration", "line-height",     "overflow",  "z-index",     "flex",
    "flex-direction", "align-items",     "justify-content", "gap",       "grid",        "transform",
    "transition",     "animation",       "opacity",         "cursor",    "float",       "box-shadow",
    "border-radius",  "outline",         "pointer-events",
};
const css_kw2 = &[_][]const u8{
    "none",        "block",   "inline", "flex",   "grid",    "absolute", "relative",
    "fixed",       "sticky",  "center", "bold",   "italic",  "normal",   "auto",
    "inherit",     "initial", "unset",  "hidden", "visible", "solid",    "dashed",
    "transparent",
};

const html_kw1 = &[_][]const u8{
    "html",   "head",    "body",    "div",     "span",   "p",        "a",
    "img",    "ul",      "ol",      "li",      "h1",     "h2",       "h3",
    "h4",     "h5",      "h6",      "table",   "tr",     "td",       "th",
    "form",   "input",   "button",  "select",  "option", "textarea", "label",
    "script", "style",   "link",    "meta",    "title",  "header",   "footer",
    "nav",    "main",    "section", "article", "aside",  "pre",      "code",
    "strong", "em",      "br",      "hr",      "iframe", "canvas",   "video",
    "audio",  "details", "summary", "dialog",
};
const html_kw2 = &[_][]const u8{
    "class",  "id",     "src",     "href",        "type",     "name",     "value",
    "style",  "action", "method",  "placeholder", "required", "disabled", "checked",
    "alt",    "rel",    "charset", "content",     "lang",     "role",     "width",
    "height",
};

const make_kw1 = &[_][]const u8{
    "ifeq",   "ifneq", "ifdef",  "ifndef",   "else",     "endif", "include",
    "define", "endef", "export", "unexport", "override", "vpath",
};
const make_kw2 = &[_][]const u8{ "PHONY", "SUFFIXES", "DEFAULT_GOAL", "FORCE" };

const empty_kw = &[_][]const u8{};

pub const HLDB = [_]SyntaxDef{
    .{ .filetype = "C/C++", .extensions = &.{ ".c", ".h", ".cpp", ".cc", ".cxx", ".hpp" }, .keywords1 = c_kw1, .keywords2 = c_kw2, .singleline_comment = "//", .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Python", .extensions = &.{ ".py", ".pyw" }, .keywords1 = py_kw1, .keywords2 = py_kw2, .singleline_comment = "#", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "JS/TS", .extensions = &.{ ".js", ".mjs", ".cjs", ".jsx", ".ts", ".tsx", ".mts" }, .keywords1 = js_kw1, .keywords2 = js_kw2, .singleline_comment = "//", .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Rust", .extensions = &.{".rs"}, .keywords1 = rs_kw1, .keywords2 = rs_kw2, .singleline_comment = "//", .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Go", .extensions = &.{".go"}, .keywords1 = go_kw1, .keywords2 = go_kw2, .singleline_comment = "//", .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Shell", .extensions = &.{ ".sh", ".bash", ".zsh" }, .keywords1 = sh_kw1, .keywords2 = sh_kw2, .singleline_comment = "#", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "JSON", .extensions = &.{ ".json", ".jsonc" }, .keywords1 = empty_kw, .keywords2 = json_kw2, .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "YAML", .extensions = &.{ ".yaml", ".yml" }, .keywords1 = empty_kw, .keywords2 = yaml_kw2, .singleline_comment = "#", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "TOML", .extensions = &.{".toml"}, .keywords1 = empty_kw, .keywords2 = toml_kw2, .singleline_comment = "#", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Markdown", .extensions = &.{ ".md", ".markdown" }, .keywords1 = empty_kw, .keywords2 = empty_kw, .singleline_comment = "#" },
    .{ .filetype = "CSS", .extensions = &.{ ".css", ".scss", ".less" }, .keywords1 = css_kw1, .keywords2 = css_kw2, .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "HTML", .extensions = &.{ ".html", ".htm", ".xml", ".svg" }, .keywords1 = html_kw1, .keywords2 = html_kw2, .multiline_comment_start = "<!--", .multiline_comment_end = "-->", .highlight_strings = true },
    .{ .filetype = "SQL", .extensions = &.{".sql"}, .keywords1 = sql_kw1, .keywords2 = sql_kw2, .singleline_comment = "--", .multiline_comment_start = "/*", .multiline_comment_end = "*/", .highlight_numbers = true, .highlight_strings = true },
    .{ .filetype = "Makefile", .extensions = &.{ "Makefile", "makefile", ".mk" }, .keywords1 = make_kw1, .keywords2 = make_kw2, .singleline_comment = "#", .highlight_numbers = true },
    .{ .filetype = "Zig", .extensions = &.{".zig"}, .keywords1 = zig_kw1, .keywords2 = zig_kw2, .singleline_comment = "//", .highlight_numbers = true, .highlight_strings = true },
};

/// Select syntax definition based on filename extension.
pub fn selectSyntax(filename: ?[]const u8) ?*const SyntaxDef {
    const name = filename orelse return null;

    // Extract extension
    const ext = blk: {
        var i = name.len;
        while (i > 0) {
            i -= 1;
            if (name[i] == '.') break :blk name[i..];
        }
        break :blk name; // no extension — try full name match (for Makefile)
    };

    for (&HLDB) |*entry| {
        for (entry.extensions) |pattern| {
            if (pattern[0] == '.') {
                // Extension match
                if (std.mem.eql(u8, ext, pattern)) return entry;
            } else {
                // Full name match (Makefile)
                if (std.mem.endsWith(u8, name, pattern)) return entry;
            }
        }
    }
    return null;
}

pub fn hlToColor(hl: HlType, cfg: *const Config) Color {
    return switch (hl) {
        .normal => .none,
        .comment, .mlcomment => cfg.color_comment,
        .keyword1 => cfg.color_keyword1,
        .keyword2 => cfg.color_keyword2,
        .string => cfg.color_string,
        .number => cfg.color_number,
        .match => cfg.color_match,
    };
}

/// Compute per-character highlight types for a rendered (tab-expanded) line.
/// Returns the multiline-comment state at the end of the line.
pub fn computeHighlights(
    syn: *const SyntaxDef,
    rendered: []const u8,
    hl: []HlType,
    in_comment_start: bool,
) bool {
    const scs = syn.singleline_comment;
    const mcs = syn.multiline_comment_start;
    const mce = syn.multiline_comment_end;
    const scs_len = if (scs) |s| s.len else 0;
    const mcs_len = if (mcs) |s| s.len else 0;
    const mce_len = if (mce) |s| s.len else 0;

    @memset(hl, .normal);

    var prev_sep: bool = true;
    var in_string: u8 = 0;
    var in_comment = in_comment_start;

    var i: usize = 0;
    while (i < rendered.len) {
        const c = rendered[i];
        const prev_hl: HlType = if (i > 0) hl[i - 1] else .normal;

        // Single-line comment
        if (scs_len > 0 and in_string == 0 and !in_comment) {
            if (i + scs_len <= rendered.len and std.mem.eql(u8, rendered[i..][0..scs_len], scs.?)) {
                @memset(hl[i..], .comment);
                break;
            }
        }

        // Multiline comment
        if (mcs_len > 0 and mce_len > 0 and in_string == 0) {
            if (in_comment) {
                hl[i] = .mlcomment;
                if (i + mce_len <= rendered.len and std.mem.eql(u8, rendered[i..][0..mce_len], mce.?)) {
                    @memset(hl[i..][0..mce_len], .mlcomment);
                    i += mce_len;
                    in_comment = false;
                    prev_sep = true;
                    continue;
                }
                i += 1;
                continue;
            } else if (i + mcs_len <= rendered.len and std.mem.eql(u8, rendered[i..][0..mcs_len], mcs.?)) {
                @memset(hl[i..][0..mcs_len], .mlcomment);
                i += mcs_len;
                in_comment = true;
                continue;
            }
        }

        // Strings
        if (syn.highlight_strings) {
            if (in_string != 0) {
                hl[i] = .string;
                if (c == '\\' and i + 1 < rendered.len) {
                    hl[i + 1] = .string;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i += 1;
                prev_sep = true;
                continue;
            } else if ((c == '"' or c == '\'') and prev_sep) {
                in_string = c;
                hl[i] = .string;
                i += 1;
                continue;
            }
        }

        // Numbers
        if (syn.highlight_numbers) {
            // Hex: 0x...
            if (c == '0' and i + 1 < rendered.len and
                (rendered[i + 1] == 'x' or rendered[i + 1] == 'X') and prev_sep)
            {
                hl[i] = .number;
                hl[i + 1] = .number;
                i += 2;
                while (i < rendered.len and std.ascii.isHex(rendered[i])) {
                    hl[i] = .number;
                    i += 1;
                }
                prev_sep = false;
                continue;
            }
            if ((std.ascii.isDigit(c) and (prev_sep or prev_hl == .number)) or
                (c == '.' and prev_hl == .number))
            {
                hl[i] = .number;
                i += 1;
                prev_sep = false;
                continue;
            }
        }

        // Keywords
        if (prev_sep) {
            var matched = false;
            for (syn.keywords1) |kw| {
                if (i + kw.len <= rendered.len and
                    std.mem.eql(u8, rendered[i..][0..kw.len], kw) and
                    (i + kw.len >= rendered.len or isSeparator(rendered[i + kw.len])))
                {
                    @memset(hl[i..][0..kw.len], .keyword1);
                    i += kw.len;
                    prev_sep = false;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;

            for (syn.keywords2) |kw| {
                if (i + kw.len <= rendered.len and
                    std.mem.eql(u8, rendered[i..][0..kw.len], kw) and
                    (i + kw.len >= rendered.len or isSeparator(rendered[i + kw.len])))
                {
                    @memset(hl[i..][0..kw.len], .keyword2);
                    i += kw.len;
                    prev_sep = false;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;
        }

        prev_sep = isSeparator(c);
        i += 1;
    }

    return in_comment;
}

/// Update hl_open_comment for all rows by scanning through the piece table.
pub fn updateCommentState(
    syntax: ?*const SyntaxDef,
    rows: []RowMeta,
    text: anytype,
) void {
    const syn = syntax orelse return;
    const mcs = syn.multiline_comment_start orelse return;
    const mce = syn.multiline_comment_end orelse return;

    var in_comment = false;
    var line_buf: [8192]u8 = undefined;

    for (rows) |*row| {
        row.hl_open_comment = in_comment;

        const len = @min(row.size, line_buf.len);
        text.copyRange(row.off, line_buf[0..len]);
        const line = line_buf[0..len];

        var i: usize = 0;
        var in_str: u8 = 0;
        while (i < len) {
            const c = line[i];

            if (in_comment) {
                if (i + mce.len <= len and std.mem.eql(u8, line[i..][0..mce.len], mce)) {
                    in_comment = false;
                    i += mce.len;
                } else {
                    i += 1;
                }
                continue;
            }

            // single line comment
            if (syn.singleline_comment) |scs| {
                if (in_str == 0 and i + scs.len <= len and std.mem.eql(u8, line[i..][0..scs.len], scs)) {
                    break;
                }
            }

            if (in_str != 0) {
                if (c == '\\' and i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (c == in_str) in_str = 0;
                i += 1;
                continue;
            }

            if (c == '"' or c == '\'') {
                in_str = c;
                i += 1;
                continue;
            }

            if (i + mcs.len <= len and std.mem.eql(u8, line[i..][0..mcs.len], mcs)) {
                in_comment = true;
                i += mcs.len;
                continue;
            }

            i += 1;
        }
    }
}

// RowMeta is imported here for the updateCommentState signature
const RowMeta = @import("Editor.zig").RowMeta;
