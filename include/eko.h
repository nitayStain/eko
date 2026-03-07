#ifndef EKO_H
#define EKO_H

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <termios.h>
#include <time.h>

#define EKO_WELCOME "Welcome to EKo (Extended Kilo)"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editor_key {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
    SHIFT_ARROW_LEFT,
    SHIFT_ARROW_RIGHT,
    SHIFT_ARROW_UP,
    SHIFT_ARROW_DOWN
};

struct window_size {
    int cols;
    int rows;
};

struct cursor_position {
    int x;
    int y;
};

typedef enum {
    PT_ORIG = 0,
    PT_ADD  = 1,
} pt_source;

typedef struct {
    unsigned char src;
    int off;
    int len;
} pt_piece;

typedef struct {
    char *orig;
    int orig_len;

    char *add;
    int add_len;
    int add_cap;

    pt_piece *pieces;
    int piece_count;
    int piece_cap;

    int len;
} piece_table;

typedef struct editor_row {
    int idx;
    int off;
    int size;
    int render_size;
    char *str;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
    int str_dirty;
} editor_row;

struct editor_syntax;

struct editor_config {
    int rows;
    int dirty;
    struct editor_syntax *syntax;
    int row_offset;
    int col_offset;
    int render_x;
    struct cursor_position cpos;
    struct window_size win_size;
    struct termios default_termios;
    piece_table text;
    editor_row *row;
    char status[80];
    time_t status_time;

    int line_num_width;

    /* Selection state */
    int selecting;
    int mark_x;
    int mark_y;

    /* Clipboard */
    char *clipboard;
    int clipboard_len;
};

extern struct editor_config editor;
extern char *file_name;

#endif
