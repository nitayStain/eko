/*
 * Syntax highlighting engine.
 *
 * Built-in language databases are defined below. Additional languages can be
 * loaded from ~/.eko/syntax/ (.syn files) with the following format:
 *
 *   filetype             = OCaml
 *   filematch            = .ml .mli
 *   comment_single       = (*
 *   comment_multi_start  = (*
 *   comment_multi_end    = *)
 *   flags                = numbers strings
 *   keyword1             = if then else match with fun let in
 *   keyword2             = int string float bool unit
 *
 * keyword2 words get a trailing | appended internally.
 * Multiple keyword1/keyword2 lines are merged.
 */

#include "eko.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#include "config.h"
#include "syntax.h"
#include "editor.h"

static char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", NULL };
static char *C_HL_keywords[] = {
    "auto","break","case","continue","default","do","else","enum",
    "extern","for","goto","if","inline","register","restrict",
    "return","sizeof","static","struct","switch","typedef","union",
    "volatile","while",
    "class","namespace","template","new","delete","try","catch",
    "throw","override","final","public","private","protected",
    "virtual","explicit","friend","operator","using",
    "#include","#define","#undef","#ifdef","#ifndef","#if",
    "#elif","#else","#endif","#pragma","#error",
    "char|","double|","float|","int|","long|","short|",
    "unsigned|","signed|","void|","bool|","size_t|","ssize_t|",
    "uint8_t|","uint16_t|","uint32_t|","uint64_t|",
    "int8_t|","int16_t|","int32_t|","int64_t|",
    "NULL|","nullptr|","true|","false|",
    NULL
};

static char *PY_HL_extensions[] = { ".py", ".pyw", NULL };
static char *PY_HL_keywords[] = {
    "False","None","True","and","as","assert","async","await",
    "break","class","continue","def","del","elif","else","except",
    "finally","for","from","global","if","import","in","is",
    "lambda","nonlocal","not","or","pass","raise","return","try",
    "while","with","yield",
    "int|","str|","float|","bool|","list|","dict|","set|",
    "tuple|","bytes|","bytearray|","type|","object|","print|",
    "len|","range|","enumerate|","zip|","map|","filter|",
    "open|","super|","property|","classmethod|","staticmethod|",
    NULL
};

static char *JS_HL_extensions[] = {
    ".js",".mjs",".cjs",".jsx",".ts",".tsx",".mts", NULL
};
static char *JS_HL_keywords[] = {
    "break","case","catch","continue","debugger","default","delete",
    "do","else","export","extends","finally","for","function","if",
    "import","in","instanceof","new","return","super","switch",
    "this","throw","try","typeof","var","void","while","with",
    "yield","let","const","async","await","class","of","from",
    "static","get","set",
    "string|","number|","boolean|","object|","null|","undefined|",
    "void|","never|","any|","unknown|","symbol|","bigint|",
    "true|","false|","Array|","Promise|","Map|","Set|",
    "interface|","type|","enum|","namespace|","declare|",
    NULL
};

static char *RS_HL_extensions[] = { ".rs", NULL };
static char *RS_HL_keywords[] = {
    "as","async","await","break","const","continue","crate","dyn",
    "else","enum","extern","fn","for","if","impl","in","let",
    "loop","match","mod","move","mut","pub","ref","return",
    "self","Self","static","struct","super","trait","type",
    "unsafe","use","where","while",
    "i8|","i16|","i32|","i64|","i128|","isize|",
    "u8|","u16|","u32|","u64|","u128|","usize|",
    "f32|","f64|","bool|","char|","str|",
    "String|","Vec|","Option|","Result|","Box|","Rc|","Arc|",
    "true|","false|","None|","Some|","Ok|","Err|",
    NULL
};

static char *GO_HL_extensions[] = { ".go", NULL };
static char *GO_HL_keywords[] = {
    "break","case","chan","const","continue","default","defer",
    "else","fallthrough","for","func","go","goto","if","import",
    "interface","map","package","range","return","select","struct",
    "switch","type","var",
    "bool|","byte|","complex64|","complex128|","error|",
    "float32|","float64|","int|","int8|","int16|","int32|","int64|",
    "rune|","string|","uint|","uint8|","uint16|","uint32|","uint64|",
    "uintptr|","nil|","true|","false|","iota|",
    "make|","new|","len|","cap|","append|","copy|","close|",
    "delete|","panic|","recover|",
    NULL
};

static char *SH_HL_extensions[] = { ".sh", ".bash", ".zsh", NULL };
static char *SH_HL_keywords[] = {
    "if","then","else","elif","fi","for","in","do","done",
    "while","until","case","esac","function","return","exit",
    "break","continue","local","readonly","export","unset",
    "shift","set",
    "echo|","printf|","read|","cd|","pwd|","ls|","mkdir|",
    "rm|","mv|","cp|","cat|","grep|","sed|","awk|","cut|",
    "source|","eval|","exec|","test|",
    NULL
};

static char *JSON_HL_extensions[] = { ".json", ".jsonc", NULL };
static char *JSON_HL_keywords[] = { "true|","false|","null|", NULL };

static char *YAML_HL_extensions[] = { ".yaml", ".yml", NULL };
static char *YAML_HL_keywords[] = {
    "true|","false|","null|","yes|","no|","on|","off|",
    "True|","False|","Null|","Yes|","No|","On|","Off|",
    NULL
};

static char *TOML_HL_extensions[] = { ".toml", NULL };
static char *TOML_HL_keywords[] = { "true|","false|", NULL };

static char *MD_HL_extensions[] = { ".md", ".markdown", NULL };
static char *MD_HL_keywords[] = { NULL };

static char *CSS_HL_extensions[] = { ".css", ".scss", ".less", NULL };
static char *CSS_HL_keywords[] = {
    "color","background","border","margin","padding","display",
    "width","height","max-width","min-width","max-height","min-height",
    "position","top","left","right","bottom","font-size","font-family",
    "font-weight","font-style","text-align","text-decoration","line-height",
    "letter-spacing","white-space","word-break","overflow","z-index",
    "flex","flex-direction","flex-wrap","align-items","align-self",
    "justify-content","justify-self","gap","grid","grid-template",
    "grid-column","grid-row","transform","transition","animation",
    "opacity","visibility","cursor","content","float","clear",
    "box-shadow","border-radius","outline","pointer-events",
    "none|","block|","inline|","inline-block|","inline-flex|",
    "flex|","grid|","absolute|","relative|","fixed|","sticky|","static|",
    "center|","left|","right|","top|","bottom|","middle|",
    "bold|","italic|","normal|","underline|","auto|",
    "inherit|","initial|","unset|","revert|",
    "hidden|","visible|","scroll|","clip|",
    "solid|","dashed|","dotted|","double|","transparent|",
    "row|","column|","wrap|","nowrap|","stretch|","space-between|",
    "space-around|","space-evenly|",
    NULL
};

static char *HTML_HL_extensions[] = { ".html", ".htm", ".xml", ".svg", NULL };
static char *HTML_HL_keywords[] = {
    "html","head","body","div","span","p","a","img","ul","ol","li",
    "h1","h2","h3","h4","h5","h6","table","tr","td","th","thead","tbody",
    "form","input","button","select","option","textarea","label",
    "script","style","link","meta","title","header","footer","nav",
    "main","section","article","aside","figure","figcaption",
    "strong","em","code","pre","blockquote","br","hr",
    "iframe","canvas","video","audio","source","details","summary","dialog",
    "class|","id|","src|","href|","type|","name|","value|","style|",
    "action|","method|","placeholder|","required|","disabled|",
    "checked|","selected|","readonly|","hidden|","multiple|",
    "alt|","rel|","charset|","content|","lang|","role|",
    "width|","height|","colspan|","rowspan|","tabindex|",
    NULL
};

static char *SQL_HL_extensions[] = { ".sql", NULL };
static char *SQL_HL_keywords[] = {
    "SELECT","FROM","WHERE","AND","OR","NOT","IN","IS","LIKE","EXISTS",
    "ORDER","BY","GROUP","HAVING","LIMIT","OFFSET","DISTINCT","ALL",
    "JOIN","INNER","LEFT","RIGHT","FULL","OUTER","CROSS","ON","USING","AS",
    "UNION","INTERSECT","EXCEPT",
    "INSERT","INTO","VALUES","UPDATE","SET","DELETE",
    "CREATE","TABLE","INDEX","VIEW","DATABASE","SCHEMA",
    "DROP","ALTER","ADD","COLUMN","RENAME","TRUNCATE",
    "CONSTRAINT","PRIMARY","KEY","FOREIGN","REFERENCES",
    "UNIQUE","DEFAULT","CHECK","AUTO_INCREMENT","SERIAL",
    "BEGIN","COMMIT","ROLLBACK","TRANSACTION","SAVEPOINT",
    "CASE","WHEN","THEN","ELSE","END","BETWEEN","CAST","COALESCE",
    "select","from","where","and","or","not","in","is","like","exists",
    "order","by","group","having","limit","offset","distinct","all",
    "join","inner","left","right","full","outer","cross","on","using","as",
    "union","intersect","except",
    "insert","into","values","update","set","delete",
    "create","table","index","view","database","schema",
    "drop","alter","add","column","rename","truncate",
    "begin","commit","rollback","transaction","savepoint",
    "case","when","then","else","end","between","cast","coalesce",
    "INT|","INTEGER|","BIGINT|","SMALLINT|","TINYINT|","SERIAL|",
    "FLOAT|","DOUBLE|","DECIMAL|","NUMERIC|","REAL|",
    "VARCHAR|","CHAR|","TEXT|","BLOB|","CLOB|","JSON|","JSONB|",
    "DATE|","DATETIME|","TIMESTAMP|","TIME|","YEAR|",
    "BOOLEAN|","BOOL|","BIT|","BYTEA|",
    "int|","integer|","bigint|","smallint|","tinyint|","serial|",
    "float|","double|","decimal|","numeric|","real|",
    "varchar|","char|","text|","blob|","clob|","json|","jsonb|",
    "date|","datetime|","timestamp|","time|","boolean|","bool|",
    NULL
};

static char *MAKE_HL_extensions[] = { "Makefile", "makefile", ".mk", NULL };
static char *MAKE_HL_keywords[] = {
    "ifeq","ifneq","ifdef","ifndef","else","endif",
    "include","define","endef","export","unexport","override","vpath",
    "PHONY|","SUFFIXES|","DEFAULT_GOAL|","FORCE|",
    NULL
};

static struct editor_syntax HLDB[] = {
    { "C/C++",    C_HL_extensions,    C_HL_keywords,    "//", "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Python",   PY_HL_extensions,   PY_HL_keywords,   "#",  NULL,   NULL,  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "JS/TS",    JS_HL_extensions,   JS_HL_keywords,   "//", "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Rust",     RS_HL_extensions,   RS_HL_keywords,   "//", "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Go",       GO_HL_extensions,   GO_HL_keywords,   "//", "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Shell",    SH_HL_extensions,   SH_HL_keywords,   "#",  NULL,   NULL,  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "JSON",     JSON_HL_extensions, JSON_HL_keywords,  NULL, NULL,   NULL,  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "YAML",     YAML_HL_extensions, YAML_HL_keywords,  "#",  NULL,   NULL,  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "TOML",     TOML_HL_extensions, TOML_HL_keywords,  "#",  NULL,   NULL,  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Markdown", MD_HL_extensions,   MD_HL_keywords,    "#",  NULL,   NULL,  0 },
    { "CSS",      CSS_HL_extensions,  CSS_HL_keywords,   NULL, "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "HTML",     HTML_HL_extensions, HTML_HL_keywords,  NULL, "<!--", "-->", HL_HIGHLIGHT_STRINGS },
    { "SQL",      SQL_HL_extensions,  SQL_HL_keywords,   "--", "/*",   "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "Makefile", MAKE_HL_extensions, MAKE_HL_keywords,  "#",  NULL,   NULL,  HL_HIGHLIGHT_NUMBERS },
};
#define HLDB_ENTRIES (int)(sizeof(HLDB) / sizeof(HLDB[0]))

static struct editor_syntax *dyn_hldb       = NULL;
static int                   dyn_hldb_count = 0;

static void rtrim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int is_separator(int c) {
    return c == '\0' || isspace(c) ||
           strchr(",.()+-/*=~%<>[];{}|&^!\"'`?:\\@#", c) != NULL;
}

#define MAX_SYN_FILEMATCH 32
#define MAX_SYN_KEYWORDS  1024

static void load_syn_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char  filetype[64]                   = "";
    char *filematch[MAX_SYN_FILEMATCH + 1]; int fm_n = 0;
    char *keywords[MAX_SYN_KEYWORDS  + 1]; int kw_n = 0;
    char  sl_comment[32]                 = "";
    char  ml_start[32]                   = "";
    char  ml_end[32]                     = "";
    int   flags                          = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *s = ltrim(line);
        rtrim(s);
        if (*s == '\0') continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key = s;
        char *val = ltrim(eq + 1);
        rtrim(key);
        rtrim(val);
        if (*key == '\0' || *val == '\0') continue;

        if (!strcmp(key, "filetype")) {
            strncpy(filetype, val, sizeof(filetype) - 1);
        } else if (!strcmp(key, "filematch")) {
            char *tok = strtok(val, " \t");
            while (tok && fm_n < MAX_SYN_FILEMATCH) {
                filematch[fm_n++] = strdup(tok);
                tok = strtok(NULL, " \t");
            }
        } else if (!strcmp(key, "comment_single")) {
            strncpy(sl_comment, val, sizeof(sl_comment) - 1);
        } else if (!strcmp(key, "comment_multi_start")) {
            strncpy(ml_start, val, sizeof(ml_start) - 1);
        } else if (!strcmp(key, "comment_multi_end")) {
            strncpy(ml_end, val, sizeof(ml_end) - 1);
        } else if (!strcmp(key, "flags")) {
            if (strstr(val, "numbers")) flags |= HL_HIGHLIGHT_NUMBERS;
            if (strstr(val, "strings")) flags |= HL_HIGHLIGHT_STRINGS;
        } else if (!strcmp(key, "keyword1")) {
            char *tok = strtok(val, " \t");
            while (tok && kw_n < MAX_SYN_KEYWORDS) {
                keywords[kw_n++] = strdup(tok);
                tok = strtok(NULL, " \t");
            }
        } else if (!strcmp(key, "keyword2")) {
            char *tok = strtok(val, " \t");
            while (tok && kw_n < MAX_SYN_KEYWORDS) {
                int tlen = (int)strlen(tok);
                char *kw = malloc(tlen + 2);
                memcpy(kw, tok, tlen);
                kw[tlen] = '|'; kw[tlen + 1] = '\0';
                keywords[kw_n++] = kw;
                tok = strtok(NULL, " \t");
            }
        }
    }
    fclose(fp);

    if (!filetype[0] || fm_n == 0) {
        for (int i = 0; i < fm_n; i++) free(filematch[i]);
        for (int i = 0; i < kw_n; i++) free(keywords[i]);
        return;
    }

    filematch[fm_n] = NULL;
    keywords[kw_n]  = NULL;

    char **fm = malloc(sizeof(char *) * (fm_n + 1));
    memcpy(fm, filematch, sizeof(char *) * (fm_n + 1));

    char **kw = malloc(sizeof(char *) * (kw_n + 1));
    memcpy(kw, keywords, sizeof(char *) * (kw_n + 1));

    dyn_hldb = realloc(dyn_hldb, sizeof(struct editor_syntax) * (dyn_hldb_count + 1));
    struct editor_syntax *e = &dyn_hldb[dyn_hldb_count++];
    e->filetype                 = strdup(filetype);
    e->filematch                = fm;
    e->keywords                 = kw;
    e->singleline_comment_start = sl_comment[0] ? strdup(sl_comment) : NULL;
    e->multiline_comment_start  = ml_start[0]   ? strdup(ml_start)   : NULL;
    e->multiline_comment_end    = ml_end[0]      ? strdup(ml_end)     : NULL;
    e->flags                    = flags;
}

void syntax_load_external(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.eko/syntax", home);

    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        int len = (int)strlen(name);
        if (len < 5 || strcmp(&name[len - 4], ".syn") != 0) continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        load_syn_file(path);
    }
    closedir(d);
}

int editor_syntax_to_color(int hl) {
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT: return config.color_comment;
        case HL_KEYWORD1:  return config.color_keyword1;
        case HL_KEYWORD2:  return config.color_keyword2;
        case HL_STRING:    return config.color_string;
        case HL_NUMBER:    return config.color_number;
        case HL_MATCH:     return config.color_match;
        default:           return EKO_COLOR_NONE;
    }
}

void editor_update_syntax(editor_row *row) {
    row->hl = realloc(row->hl, row->render_size);
    memset(row->hl, HL_NORMAL, row->render_size);

    struct editor_syntax *s = editor.syntax;
    if (!s) return;

    char **keywords = s->keywords;
    const char *scs = s->singleline_comment_start;
    const char *mcs = s->multiline_comment_start;
    const char *mce = s->multiline_comment_end;
    int scs_len = scs ? (int)strlen(scs) : 0;
    int mcs_len = mcs ? (int)strlen(mcs) : 0;
    int mce_len = mce ? (int)strlen(mce) : 0;

    int prev_sep   = 1;
    int in_string  = 0;
    int in_comment = (row->idx > 0 && editor.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->render_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->render_size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                i++;
                continue;
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (s->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->render_size) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else if ((c == '"' || c == '\'') && prev_sep) {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }

        if (s->flags & HL_HIGHLIGHT_NUMBERS) {
            if (c == '0' && i + 1 < row->render_size &&
                (row->render[i+1] == 'x' || row->render[i+1] == 'X') && prev_sep) {
                row->hl[i] = row->hl[i + 1] = HL_NUMBER;
                i += 2;
                while (i < row->render_size && isxdigit((unsigned char)row->render[i]))
                    row->hl[i++] = HL_NUMBER;
                prev_sep = 0;
                continue;
            }
            if ((isdigit((unsigned char)c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int matched = 0;
            for (int k = 0; keywords[k]; k++) {
                int klen = (int)strlen(keywords[k]);
                int kw2  = (keywords[k][klen - 1] == '|');
                if (kw2) klen--;

                if (!strncmp(&row->render[i], keywords[k], klen) &&
                    is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    prev_sep = 0;
                    matched = 1;
                    break;
                }
            }
            if (matched) continue;
        }

        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < editor.rows)
        editor_update_syntax(&editor.row[row->idx + 1]);
}

static void try_apply(struct editor_syntax *s, const char *ext) {
    for (int i = 0; s->filematch[i]; i++) {
        int is_ext = (s->filematch[i][0] == '.');
        if ((is_ext  && ext && !strcmp(ext, s->filematch[i])) ||
            (!is_ext && strstr(file_name, s->filematch[i]))) {
            editor.syntax = s;
            for (int row = 0; row < editor.rows; row++)
                editor_update_syntax(&editor.row[row]);
        }
    }
}

void editor_select_syntax_highlight(void) {
    editor.syntax = NULL;
    if (!file_name) return;

    const char *ext = strrchr(file_name, '.');

    for (int j = 0; j < dyn_hldb_count; j++)
        try_apply(&dyn_hldb[j], ext);

    if (!editor.syntax) {
        for (int j = 0; j < HLDB_ENTRIES; j++)
            try_apply(&HLDB[j], ext);
    }
}
