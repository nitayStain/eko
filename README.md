# EKo — Extended Kilo

A lightweight terminal text editor written in C, extended from [kilo](https://github.com/antirez/kilo).
EKo adds syntax highlighting for 14 languages, configurable themes, find & replace,
file switching, and a piece-table–based text buffer — all in a single dependency-free binary.

## Building

```
make
```

Requires a C99 compiler (GCC, Clang, etc.). No external libraries needed.

## Installation

```
sudo make install            # installs to /usr/local/bin
sudo make PREFIX=/usr install  # or pick a different prefix
```

## Usage

```
eko <filename>    # open a file
eko               # start with an empty buffer
```

### Keyboard shortcuts

| Key | Action |
|---|---|
| `Ctrl-S` | Save (prompts for filename if none set) |
| `Ctrl-F` | Find (incremental search) |
| `Ctrl-R` | Find & Replace (all occurrences) |
| `Ctrl-O` | Open / switch to a different file |
| `Ctrl-Q` | Quit (press twice to discard unsaved changes) |
| `Ctrl-H` / `Backspace` | Delete character before cursor |
| `Delete` | Delete character under cursor |
| `Enter` | Insert newline |
| `Home` / `End` | Jump to start / end of line |
| `Page Up` / `Page Down` | Scroll by screen |
| `Arrow keys` | Move cursor |
| `Tab` | Insert tab (or spaces if `expand_tabs` is on) |

### Find mode

While in find mode (`Ctrl-F`):

| Key | Action |
|---|---|
| Type characters | Build search query (live incremental search) |
| `Enter` / `→` / `↓` | Jump to next match |
| `Backspace` / `←` / `↑` | Jump to previous match |
| `Delete` | Remove last character from query |
| `Escape` | Cancel search and restore original position |

### Find & Replace

`Ctrl-R` prompts for a search term and then a replacement string.
All occurrences are replaced at once. Press `Escape` at either prompt to cancel.

### Switching files

`Ctrl-O` prompts for a file path. If the current buffer has unsaved changes you'll
be asked whether to save first. The new file is loaded (or an empty buffer is created
if the path doesn't exist yet).

## Syntax highlighting

EKo highlights **14 languages** out of the box:

C/C++, Python, JavaScript/TypeScript, Rust, Go, Shell (Bash/Zsh),
JSON, YAML, TOML, Markdown, CSS/SCSS/LESS, HTML/XML/SVG, SQL, and Makefile.

Languages are detected by file extension.

### Adding custom languages

Create `.syn` files in `~/.eko/syntax/`:

```ini
filetype             = OCaml
filematch            = .ml .mli
comment_single       = (*
comment_multi_start  = (*
comment_multi_end    = *)
flags                = numbers strings
keyword1             = if then else match with fun let in
keyword2             = int string float bool unit
```

- `keyword1` — highlighted as keywords (e.g. control flow)
- `keyword2` — highlighted as types (appended with `|` internally)
- `flags` — `numbers` and/or `strings` to enable literal highlighting
- Multiple `keyword1` / `keyword2` lines are merged

## Configuration

EKo reads `~/.ekorc` on startup. The file uses a simple `key = value` format.
Lines starting with `#` are comments.

### Example `~/.ekorc`

```ini
# Tab behavior
tab_size    = 4
expand_tabs = true

# Color theme
theme       = monokai

# Override individual colors with hex (#RRGGBB) or 256-color index
# color_comment  = #75715e
# color_keyword1 = #f92672
# color_keyword2 = #66d9ef
# color_string   = #e6db74
# color_number   = #ae81ff
# color_match    = #e6db74
# color_bg       = #272822
```

### Configuration keys

| Key | Type | Default | Description |
|---|---|---|---|
| `tab_size` | integer | `4` | Width of a tab stop in spaces |
| `expand_tabs` | boolean | `false` | Insert spaces instead of a tab character |
| `theme` | string | `default` | Color theme name (see below) |
| `color_bg` | color | `-1` | Background color (`-1` = terminal default) |
| `color_comment` | color | `6` | Comment color |
| `color_keyword1` | color | `3` | Keyword color |
| `color_keyword2` | color | `2` | Type / secondary keyword color |
| `color_string` | color | `5` | String literal color |
| `color_number` | color | `1` | Number literal color |
| `color_match` | color | `4` | Search match highlight color |

Boolean values accept: `true`, `yes`, `on`, `1` (anything else is false).

Color values accept three formats:
- **`#RRGGBB`** — 24-bit true color (e.g. `#f92672`). Works on all modern terminals.
- **`0`–`255`** — [256-color ANSI index](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit).
- **`-1`** — terminal default (for `color_bg` only).

Built-in themes use true color by default. The `default` theme uses basic ANSI colors
for maximum compatibility.

### Built-in themes

| Theme | Background |
|---|---|
| `default` | Terminal default (ANSI colors, max compatibility) |
| `monokai` | `#272822` |
| `dracula` | `#282a36` |
| `nord` | `#2e3440` |
| `gruvbox` | `#282828` |
| `solarized-dark` | `#002b36` |
| `one-dark` | `#282c34` |
| `retrobox` | `#1d2021` |

All themes except `default` use 24-bit true color.

Set a theme in `~/.ekorc`:

```ini
theme = dracula
```

Individual `color_*` keys override the theme's values when both are present.

### Custom themes

Create theme files in `~/.eko/themes/` with the `.theme` extension.
They use the same `key = value` format with only color keys:

```ini
# ~/.eko/themes/my-theme.theme
color_bg       = #1a1b26
color_comment  = #565f89
color_keyword1 = #bb9af7
color_keyword2 = #7aa2f7
color_string   = #9ece6a
color_number   = #ff9e64
color_match    = #e0af68
```

Then activate it:

```ini
# ~/.ekorc
theme = my-theme
```

Custom themes in `~/.eko/themes/` take priority over built-in themes of the same name.

## License

This project is based on [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo,
released under the BSD 2-Clause license.
