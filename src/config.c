/*
 * Configuration loader for ~/.ekorc
 *
 * Supported keys:
 *   tab_size    = 4
 *   expand_tabs = true
 *   theme       = monokai
 *   color_comment  = #75715e   (or 242)
 *   color_keyword1 = #f92672   (or 197)
 *   color_keyword2 = #66d9ef   (or 81)
 *   color_string   = #e6db74   (or 221)
 *   color_number   = #ae81ff   (or 141)
 *   color_match    = #e6db74   (or 226)
 *   color_bg       = #272822   (or 235, or -1 for default)
 *
 * Color values accept 256-color indices (0-255), -1 for terminal
 * default, or #RRGGBB hex for 24-bit true color.
 *
 * Built-in themes: default, monokai, dracula, nord, gruvbox,
 *                  solarized-dark, one-dark, retrobox
 *
 * Custom themes: ~/.eko/themes/NAME.theme (same key=value format)
 */

#include "eko.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

struct eko_config config;

/* ── Color escape helpers ────────────────────────────────────── */

int eko_color_fg(int color, char *buf, int bufsz) {
    if (EKO_COLOR_IS_NONE(color))
        return 0;
    if (EKO_COLOR_IS_RGB(color))
        return snprintf(buf, bufsz, "\x1b[38;2;%d;%d;%dm",
                        EKO_COLOR_R(color), EKO_COLOR_G(color), EKO_COLOR_B(color));
    return snprintf(buf, bufsz, "\x1b[38;5;%dm", color);
}

int eko_color_bg(int color, char *buf, int bufsz) {
    if (EKO_COLOR_IS_NONE(color))
        return 0;
    if (EKO_COLOR_IS_RGB(color))
        return snprintf(buf, bufsz, "\x1b[48;2;%d;%d;%dm",
                        EKO_COLOR_R(color), EKO_COLOR_G(color), EKO_COLOR_B(color));
    return snprintf(buf, bufsz, "\x1b[48;5;%dm", color);
}

/* ── Parsing helpers ─────────────────────────────────────────── */

static void rtrim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int parse_bool(const char *v) {
    return !strcmp(v, "1") || !strcmp(v, "true") ||
           !strcmp(v, "yes")  || !strcmp(v, "on");
}

static int parse_color(const char *val) {
    if (val[0] == '#' && strlen(val) == 7) {
        unsigned int r, g, b;
        if (sscanf(val + 1, "%02x%02x%02x", &r, &g, &b) == 3)
            return EKO_COLOR_RGB(r, g, b);
    }
    return atoi(val);
}

/* ── Built-in themes (true color) ────────────────────────────── */

struct theme_def {
    const char *name;
    int bg;
    int comment;
    int keyword1;
    int keyword2;
    int string;
    int number;
    int match;
};

#define TC(r,g,b) EKO_COLOR_RGB(r,g,b)

static const struct theme_def builtin_themes[] = {
    { "default",        -1,   6,   3,   2,   5,   1,   4 },
    { "monokai",        TC(0x27,0x28,0x22), TC(0x75,0x71,0x5e), TC(0xf9,0x26,0x72),
                        TC(0x66,0xd9,0xef), TC(0xe6,0xdb,0x74), TC(0xae,0x81,0xff),
                        TC(0xe6,0xdb,0x74) },
    { "dracula",        TC(0x28,0x2a,0x36), TC(0x62,0x72,0xa4), TC(0xff,0x79,0xc6),
                        TC(0x50,0xfa,0x7b), TC(0xf1,0xfa,0x8c), TC(0xbd,0x93,0xf9),
                        TC(0xf1,0xfa,0x8c) },
    { "nord",           TC(0x2e,0x34,0x40), TC(0x4c,0x56,0x6a), TC(0x81,0xa1,0xc1),
                        TC(0x88,0xc0,0xd0), TC(0xa3,0xbe,0x8c), TC(0xd0,0x87,0x70),
                        TC(0xeb,0xcb,0x8b) },
    { "gruvbox",        TC(0x28,0x28,0x28), TC(0x92,0x83,0x74), TC(0xfb,0x49,0x34),
                        TC(0xfa,0xbd,0x2f), TC(0xb8,0xbb,0x26), TC(0xd3,0x86,0x9b),
                        TC(0xeb,0xcb,0x8b) },
    { "solarized-dark", TC(0x00,0x2b,0x36), TC(0x58,0x6e,0x75), TC(0x26,0x8b,0xd2),
                        TC(0x2a,0xa1,0x98), TC(0x85,0x99,0x00), TC(0xcb,0x4b,0x16),
                        TC(0xb5,0x89,0x00) },
    { "one-dark",       TC(0x28,0x2c,0x34), TC(0x5c,0x63,0x70), TC(0xc6,0x78,0xdd),
                        TC(0x61,0xaf,0xef), TC(0xd1,0x9a,0x66), TC(0xe0,0x6c,0x75),
                        TC(0xe5,0xc0,0x7b) },
    { "retrobox",       TC(0x1d,0x20,0x21), TC(0x66,0x5c,0x54), TC(0xfe,0x80,0x19),
                        TC(0xfa,0xbd,0x2f), TC(0xb8,0xbb,0x26), TC(0xd3,0x86,0x9b),
                        TC(0xfa,0xbd,0x2f) },
    { NULL,              0,   0,   0,   0,   0,   0,   0 }
};

#undef TC

static void apply_theme_colors(const struct theme_def *t) {
    config.color_bg       = t->bg;
    config.color_comment  = t->comment;
    config.color_keyword1 = t->keyword1;
    config.color_keyword2 = t->keyword2;
    config.color_string   = t->string;
    config.color_number   = t->number;
    config.color_match    = t->match;
}

static void apply_color_kv(const char *key, const char *val) {
    if      (!strcmp(key, "color_bg"))       config.color_bg       = parse_color(val);
    else if (!strcmp(key, "color_comment"))  config.color_comment  = parse_color(val);
    else if (!strcmp(key, "color_keyword1")) config.color_keyword1 = parse_color(val);
    else if (!strcmp(key, "color_keyword2")) config.color_keyword2 = parse_color(val);
    else if (!strcmp(key, "color_string"))   config.color_string   = parse_color(val);
    else if (!strcmp(key, "color_number"))   config.color_number   = parse_color(val);
    else if (!strcmp(key, "color_match"))    config.color_match    = parse_color(val);
}

/*
 * Strip comments from a config line, preserving #RRGGBB hex values.
 * A '#' is a comment if it appears before '=' or after the value with
 * preceding whitespace.
 */
static void strip_comment(char *line) {
    char *eq = strchr(line, '=');
    if (!eq) {
        /* No '=', any '#' is a comment */
        char *h = strchr(line, '#');
        if (h) *h = '\0';
        return;
    }
    /* Strip comments before the key */
    for (char *p = line; p < eq; p++) {
        if (*p == '#') { *p = '\0'; return; }
    }
    /* After '=', skip the value — a '#' preceded by whitespace is a comment */
    char *val = ltrim(eq + 1);
    /* If value starts with '#', it's a hex color; skip past it */
    char *scan = val;
    if (*scan == '#') scan++;
    /* Find next '#' — that's a trailing comment */
    char *h = strchr(scan, '#');
    if (h) *h = '\0';
}

static void parse_kv_line(char *line, char **out_key, char **out_val) {
    strip_comment(line);
    char *s = ltrim(line); rtrim(s);
    *out_key = NULL; *out_val = NULL;
    if (!*s) return;
    char *eq = strchr(s, '='); if (!eq) return;
    *eq = '\0';
    char *key = s, *val = ltrim(eq + 1);
    rtrim(key); rtrim(val);
    if (!*key || !*val) return;
    *out_key = key;
    *out_val = val;
}

static int load_theme_file(const char *name, const char *home) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/.eko/themes/%s.theme", home, name);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key, *val;
        parse_kv_line(line, &key, &val);
        if (key && val) apply_color_kv(key, val);
    }
    fclose(fp);
    return 1;
}

static void apply_theme(const char *name, const char *home) {
    if (home && load_theme_file(name, home)) return;
    for (int i = 0; builtin_themes[i].name; i++) {
        if (!strcmp(builtin_themes[i].name, name)) {
            apply_theme_colors(&builtin_themes[i]);
            return;
        }
    }
}

static void config_defaults(void) {
    config.tab_size    = 4;
    config.expand_tabs = 0;
    config.theme[0]    = '\0';
    apply_theme_colors(&builtin_themes[0]);
}

static void parse_rc_line(char *line) {
    char *key, *val;
    parse_kv_line(line, &key, &val);
    if (!key || !val) return;

    if      (!strcmp(key, "tab_size"))    config.tab_size    = atoi(val);
    else if (!strcmp(key, "expand_tabs")) config.expand_tabs = parse_bool(val);
    else apply_color_kv(key, val);
}

void config_load(void) {
    config_defaults();

    const char *home = getenv("HOME");
    if (!home) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.ekorc", home);
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    /* First pass: find theme key */
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char tmp[256];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *key, *val;
        parse_kv_line(tmp, &key, &val);
        if (key && val && !strcmp(key, "theme")) {
            strncpy(config.theme, val, sizeof(config.theme) - 1);
            break;
        }
    }

    if (config.theme[0])
        apply_theme(config.theme, home);

    /* Second pass: apply all settings (color overrides win over theme) */
    rewind(fp);
    while (fgets(line, sizeof(line), fp))
        parse_rc_line(line);

    fclose(fp);
}
