#ifndef SYNTAX_H
#define SYNTAX_H

#include "eko.h"

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

enum hl_type {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

struct editor_syntax {
    char  *filetype;
    char **filematch;
    char **keywords;
    char  *singleline_comment_start;
    char  *multiline_comment_start;
    char  *multiline_comment_end;
    int    flags;
};

int  editor_syntax_to_color(int hl);
void editor_update_syntax(editor_row *row);
void editor_select_syntax_highlight(void);
void syntax_load_external(void);

#endif
