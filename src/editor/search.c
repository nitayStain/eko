#include "editor.h"

static void editor_find_next(char *query, int qlen, int *last_row, int *last_col, int direction,
                             int *hl_line, unsigned char **hl_save) {
    if (qlen <= 0 || editor.rows == 0) return;

    if (*hl_save) {
        memcpy(editor.row[*hl_line].hl, *hl_save,
               editor.row[*hl_line].render_size);
        free(*hl_save);
        *hl_save = NULL;
        *hl_line = -1;
    }

    int start_row = *last_row;
    int start_col = *last_col;

    if (start_row >= 0) {
        if (direction == 1) {
            start_col++;
        } else {
            start_col--;
        }
    } else {
        start_row = editor.cpos.y;
        start_col = (direction == 1) ? 0 : editor.row[start_row].size;
    }

    for (int i = 0; i < editor.rows; i++) {
        int r;
        if (direction == 1) {
            r = (start_row + i) % editor.rows;
        } else {
            r = (start_row - i + editor.rows) % editor.rows;
        }

        editor_row *row = &editor.row[r];
        editor_row_ensure_flat(row);

        int from, to;
        if (i == 0) {
            from = (direction == 1) ? (start_col < 0 ? 0 : start_col) : 0;
            to = (direction == 1) ? row->size : (start_col >= 0 ? start_col + qlen : row->size);
        } else {
            from = 0;
            to = row->size;
        }
        if (from < 0) from = 0;
        if (to > row->size) to = row->size;

        if (direction == 1) {
            for (int col = from; col + qlen <= to; col++) {
                if (memcmp(&row->str[col], query, qlen) == 0) {
                    *last_row = r;
                    *last_col = col;
                    editor.cpos.y = r;
                    editor.cpos.x = col;
                    editor.row_offset = editor.rows;

                    editor_update_row(row);
                    int rx = convert_row_to_render_x(row, col);
                    if (row->hl) {
                        *hl_line = r;
                        *hl_save = malloc(row->render_size);
                        memcpy(*hl_save, row->hl, row->render_size);
                        int rend = convert_row_to_render_x(row, col + qlen);
                        memset(&row->hl[rx], HL_MATCH, rend - rx);
                    }
                    return;
                }
            }
        } else {
            for (int col = to - qlen; col >= from; col--) {
                if (memcmp(&row->str[col], query, qlen) == 0) {
                    *last_row = r;
                    *last_col = col;
                    editor.cpos.y = r;
                    editor.cpos.x = col;
                    editor.row_offset = editor.rows;

                    editor_update_row(row);
                    int rx = convert_row_to_render_x(row, col);
                    if (row->hl) {
                        *hl_line = r;
                        *hl_save = malloc(row->render_size);
                        memcpy(*hl_save, row->hl, row->render_size);
                        int rend = convert_row_to_render_x(row, col + qlen);
                        memset(&row->hl[rx], HL_MATCH, rend - rx);
                    }
                    return;
                }
            }
        }
    }
}

static void editor_replace_at(int r, int col, int qlen,
                              const char *replacement, int rlen) {
    int off = editor.row[r].off + col;
    pt_delete(&editor.text, off, qlen);
    if (rlen > 0)
        pt_insert(&editor.text, off, replacement, rlen);

    int delta = rlen - qlen;
    editor.row[r].size += delta;
    editor_row_cache_invalidate(&editor.row[r]);
    editor_shift_offsets_from(r + 1, delta);
    editor_update_row(&editor.row[r]);
    editor.dirty++;
}

void editor_find(void) {
    int saved_cx = editor.cpos.x;
    int saved_cy = editor.cpos.y;
    int saved_coloff = editor.col_offset;
    int saved_rowoff = editor.row_offset;

    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    int last_row = -1, last_col = -1;
    int hl_line = -1;
    unsigned char *hl_save = NULL;

    while (1) {
        editor_set_status_message("Search: %s (ESC=cancel Enter=next Backspace=prev)", buf);
        editor_refresh_screen();

        int c = editor_read_key();

        if (c == '\x1b') {
            if (hl_save) {
                memcpy(editor.row[hl_line].hl, hl_save,
                       editor.row[hl_line].render_size);
                free(hl_save);
            }
            editor.cpos.x = saved_cx;
            editor.cpos.y = saved_cy;
            editor.col_offset = saved_coloff;
            editor.row_offset = saved_rowoff;
            editor_set_status_message("");
            free(buf);
            return;
        } else if (c == '\r' || c == ARROW_RIGHT || c == ARROW_DOWN) {
            editor_find_next(buf, (int)buflen, &last_row, &last_col, 1,
                             &hl_line, &hl_save);
        } else if (c == BACKSPACE || c == CTRL_KEY('h') || c == ARROW_LEFT || c == ARROW_UP) {
            editor_find_next(buf, (int)buflen, &last_row, &last_col, -1,
                             &hl_line, &hl_save);
        } else if (c == DEL_KEY) {
            if (buflen != 0) {
                buf[--buflen] = '\0';
                last_row = -1;
                last_col = -1;
                if (hl_save) {
                    memcpy(editor.row[hl_line].hl, hl_save,
                           editor.row[hl_line].render_size);
                    free(hl_save);
                    hl_save = NULL;
                    hl_line = -1;
                }
                editor_find_next(buf, (int)buflen, &last_row, &last_col, 1,
                                 &hl_line, &hl_save);
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
            last_row = -1;
            last_col = -1;
            if (hl_save) {
                memcpy(editor.row[hl_line].hl, hl_save,
                       editor.row[hl_line].render_size);
                free(hl_save);
                hl_save = NULL;
                hl_line = -1;
            }
            editor_find_next(buf, (int)buflen, &last_row, &last_col, 1,
                             &hl_line, &hl_save);
        }
    }
}

void editor_find_replace(void) {
    char *query = editor_prompt("Find: %s (ESC to cancel)", NULL);
    if (!query) return;

    char *replacement = editor_prompt("Replace with: %s (ESC to cancel)", NULL);
    if (!replacement) { free(query); return; }

    int qlen = (int)strlen(query);
    int rlen = (int)strlen(replacement);
    int replaced = 0;

    int last_row = -1, last_col = -1;
    int hl_line = -1;
    unsigned char *hl_save = NULL;

    editor_find_next(query, qlen, &last_row, &last_col, 1, &hl_line, &hl_save);
    if (last_row < 0) {
        editor_set_status_message("No matches found.");
        free(query);
        free(replacement);
        return;
    }

    while (1) {
        editor_set_status_message(
            "Replace? Enter=yes Tab=skip ^A=all ESC=done (%d replaced)", replaced);
        editor_refresh_screen();

        int c = editor_read_key();

        if (c == '\x1b') {
            break;
        } else if (c == '\r') {
            if (hl_save) {
                memcpy(editor.row[hl_line].hl, hl_save, editor.row[hl_line].render_size);
                free(hl_save);
                hl_save = NULL;
                hl_line = -1;
            }
            editor_replace_at(last_row, last_col, qlen, replacement, rlen);
            replaced++;
            last_col += rlen - 1;
            editor_find_next(query, qlen, &last_row, &last_col, 1,
                             &hl_line, &hl_save);
            if (last_row < 0) {
                editor_set_status_message("Replaced %d occurrence(s). No more matches.", replaced);
                break;
            }
        } else if (c == '\t' || c == ARROW_DOWN || c == ARROW_RIGHT) {
            editor_find_next(query, qlen, &last_row, &last_col, 1,
                             &hl_line, &hl_save);
            if (last_row < 0) {
                editor_set_status_message("No more matches. %d replaced.", replaced);
                break;
            }
        } else if (c == BACKSPACE || c == ARROW_UP || c == ARROW_LEFT) {
            editor_find_next(query, qlen, &last_row, &last_col, -1,
                             &hl_line, &hl_save);
        } else if (c == CTRL_KEY('a')) {
            if (hl_save) {
                memcpy(editor.row[hl_line].hl, hl_save, editor.row[hl_line].render_size);
                free(hl_save);
                hl_save = NULL;
                hl_line = -1;
            }
            while (last_row >= 0) {
                editor_replace_at(last_row, last_col, qlen, replacement, rlen);
                replaced++;
                last_col += rlen - 1;
                editor_find_next(query, qlen, &last_row, &last_col, 1,
                                 &hl_line, &hl_save);
            }
            editor_set_status_message("Replaced %d occurrence(s).", replaced);
            break;
        }
    }

    if (hl_save) {
        memcpy(editor.row[hl_line].hl, hl_save, editor.row[hl_line].render_size);
        free(hl_save);
    }
    free(query);
    free(replacement);
}

void editor_goto_line(void) {
    char *input = editor_prompt("Go to line: %s (ESC to cancel)", NULL);
    if (!input) return;
    int line = atoi(input);
    free(input);
    if (line < 1) line = 1;
    if (line > editor.rows) line = editor.rows;
    editor.cpos.y = line - 1;
    editor.cpos.x = 0;
    editor.row_offset = editor.rows;
    editor_set_status_message("Line %d", line);
}
