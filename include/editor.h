#ifndef EDITOR_H
#define EDITOR_H

#include "eko.h"
#include "abuf.h"
#include "terminal.h"
#include "syntax.h"
#include "config.h"
#include "piece_table.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>

void editor_row_cache_invalidate(editor_row *row);
void editor_row_cache_free(editor_row *row);
void editor_row_ensure_flat(editor_row *row);
void editor_insert_row_meta(int at, int off, int size);
void editor_del_row_meta(int at);
void editor_shift_offsets_from(int from, int delta);
void editor_rebuild_rows(void);
void editor_offset_to_cursor(int off, int *cy, int *cx);

int  convert_row_to_render_x(editor_row *row, int x);
int  editor_render_x_to_cx(editor_row *row, int rx);

void editor_set_status_message(const char *fmt, ...);
void editor_update_row(editor_row *row);
void editor_update_line_num_width(void);

char *editor_prompt(char *prompt, void (*callback)(char *, int));

void editor_scroll(void);
void editor_draw_rows(struct abuf *ab);
void editor_draw_status_bar(struct abuf *ab);
void editor_draw_message_bar(struct abuf *ab);
void editor_refresh_screen(void);

int  editor_read_key(void);
void editor_move_cursor(int key);
void editor_process_keypress(void);

void editor_insert_char(int c);
void editor_insert_newline(void);
void editor_del_char(void);

void editor_find(void);
void editor_find_replace(void);
void editor_goto_line(void);

void editor_get_sel_offsets(int *start, int *end);
void editor_delete_selection(void);
void editor_select_start(void);
void editor_select_clear(void);
void editor_copy(void);
void editor_cut(void);
void editor_paste(void);

char *editor_rows_to_string(int *buflen);
void editor_save(void);
void editor_switch_file(void);
void editor_open(void);
void editor_init(void);

#endif
