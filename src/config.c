#include "eko.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

struct eko_config config;

static char config_err[256];
static int  config_err_set;

static void config_set_error(const char *file, int lineno, const char *msg) {
    if (config_err_set) return;
    snprintf(config_err, sizeof(config_err), "%s:%d: %s", file, lineno, msg);
    config_err_set = 1;
}

const char *config_error(void) {
    return config_err_set ? config_err : NULL;
}

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

static void rtrim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int parse_bool(const char *v) {
    if (!strcmp(v, "true"))  return 1;
    if (!strcmp(v, "false")) return 0;
    return -1;
}

static int is_valid_color(const char *val) {
    if (val[0] == '#') {
        if (strlen(val) != 7) return 0;
        for (int i = 1; i <= 6; i++)
            if (!isxdigit((unsigned char)val[i])) return 0;
        return 1;
    }
    if (val[0] == '-' && val[1] == '1' && val[2] == '\0') return 1;
    if (val[0] == '\0') return 0;
    for (const char *p = val; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    int n = atoi(val);
    return n >= 0 && n <= 255;
}

static int parse_color(const char *val) {
    if (val[0] == '#' && strlen(val) == 7) {
        unsigned int r, g, b;
        if (sscanf(val + 1, "%02x%02x%02x", &r, &g, &b) == 3)
            return EKO_COLOR_RGB(r, g, b);
    }
    return atoi(val);
}

static int is_color_key(const char *key) {
    return !strcmp(key, "color_bg")       || !strcmp(key, "color_comment")  ||
           !strcmp(key, "color_keyword1") || !strcmp(key, "color_keyword2") ||
           !strcmp(key, "color_string")   || !strcmp(key, "color_number")   ||
           !strcmp(key, "color_match");
}

struct theme_def {
    const char *name;
    int bg, comment, keyword1, keyword2, string, number, match;
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

static void apply_color_kv(const char *key, const char *val,
                            const char *file, int lineno) {
    if (!is_valid_color(val)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "invalid color '%s' for %s", val, key);
        config_set_error(file, lineno, msg);
        return;
    }
    int c = parse_color(val);
    if      (!strcmp(key, "color_bg"))       config.color_bg       = c;
    else if (!strcmp(key, "color_comment"))  config.color_comment  = c;
    else if (!strcmp(key, "color_keyword1")) config.color_keyword1 = c;
    else if (!strcmp(key, "color_keyword2")) config.color_keyword2 = c;
    else if (!strcmp(key, "color_string"))   config.color_string   = c;
    else if (!strcmp(key, "color_number"))   config.color_number   = c;
    else if (!strcmp(key, "color_match"))    config.color_match    = c;
}

static void strip_comment(char *line) {
    char *eq = strchr(line, '=');
    if (!eq) {
        char *h = strchr(line, '#');
        if (h) *h = '\0';
        return;
    }
    for (char *p = line; p < eq; p++) {
        if (*p == '#') { *p = '\0'; return; }
    }
    char *val = ltrim(eq + 1);
    char *scan = val;
    if (*scan == '#') scan++;
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
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *key, *val;
        parse_kv_line(line, &key, &val);
        if (key && val) {
            if (is_color_key(key)) {
                apply_color_kv(key, val, path, lineno);
            } else {
                config_set_error(path, lineno, "unknown key");
            }
        }
    }
    fclose(fp);
    return 1;
}

static int apply_theme(const char *name, const char *home,
                        const char *file, int lineno) {
    if (home && load_theme_file(name, home)) return 1;
    for (int i = 0; builtin_themes[i].name; i++) {
        if (!strcmp(builtin_themes[i].name, name)) {
            apply_theme_colors(&builtin_themes[i]);
            return 1;
        }
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "unknown theme '%s'", name);
    config_set_error(file, lineno, msg);
    return 0;
}

static void config_defaults(void) {
    config.tab_size    = 4;
    config.expand_tabs = 0;
    config.theme[0]    = '\0';
    apply_theme_colors(&builtin_themes[0]);
}

static void parse_rc_line(char *line, const char *file, int lineno) {
    char *key, *val;
    parse_kv_line(line, &key, &val);
    if (!key || !val) return;

    if (!strcmp(key, "tab_size")) {
        int n = atoi(val);
        if (n < 1 || n > 16) {
            char msg[128];
            snprintf(msg, sizeof(msg), "tab_size must be 1-16, got '%s'", val);
            config_set_error(file, lineno, msg);
        } else {
            config.tab_size = n;
        }
    } else if (!strcmp(key, "expand_tabs")) {
        int b = parse_bool(val);
        if (b < 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "expand_tabs must be true or false, got '%s'", val);
            config_set_error(file, lineno, msg);
        } else {
            config.expand_tabs = b;
        }
    } else if (!strcmp(key, "theme")) {
        /* handled in first pass */
    } else if (is_color_key(key)) {
        apply_color_kv(key, val, file, lineno);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "unknown key '%s'", key);
        config_set_error(file, lineno, msg);
    }
}

void config_load(void) {
    config_defaults();
    config_err[0] = '\0';
    config_err_set = 0;

    const char *home = getenv("HOME");
    if (!home) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.ekorc", home);
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char line[256];
    int lineno = 0;
    int theme_lineno = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char tmp[256];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *key, *val;
        parse_kv_line(tmp, &key, &val);
        if (key && val && !strcmp(key, "theme")) {
            strncpy(config.theme, val, sizeof(config.theme) - 1);
            theme_lineno = lineno;
            break;
        }
    }

    if (config.theme[0])
        apply_theme(config.theme, home, path, theme_lineno);

    rewind(fp);
    lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        parse_rc_line(line, path, lineno);
    }

    fclose(fp);
}
