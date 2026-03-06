#ifndef EDITOR_H
#define EDITOR_H

#include "eko.h"
#include "syntax.h"
#include "config.h"
#include "abuf.h"
#include <stdlib.h>

void editor_set_status_message(const char *fmt, ...);
int  editor_render_x_to_cx(editor_row *row, int rx);
char *editor_prompt(char *prompt, void (*callback)(char *, int));
void editor_find(void);
void editor_find_replace(void);
void editor_switch_file(void);
void editor_update_row(editor_row *row);
void editor_row_ensure_flat(editor_row *row);
void editor_insert_char(int c);
void editor_insert_newline(void);
void editor_del_char(void);
char *editor_rows_to_string(int *buflen);
void editor_save(void);
void editor_scroll(void);
void editor_draw_rows(struct abuf *ab);
void editor_draw_status_bar(struct abuf *ab);
void editor_draw_message_bar(struct abuf *ab);
void editor_refresh_screen(void);
int  editor_read_key(void);
void editor_move_cursor(int key);
void editor_process_keypress(void);
void editor_open(void);
void editor_init(void);

#endif
