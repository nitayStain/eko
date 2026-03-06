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
#include "eko.h"
#include "abuf.h"
#include "terminal.h"
#include "syntax.h"
#include "editor.h"
#include "piece_table.h"

struct editor_config editor;
char *file_name;

static void editor_row_cache_invalidate(editor_row *row) {
    free(row->str);
    row->str = NULL;
    free(row->render);
    row->render = NULL;
    free(row->hl);
    row->hl = NULL;
    row->render_size = 0;
    row->str_dirty = 1;
}

static void editor_row_cache_free(editor_row *row) {
    free(row->str);
    free(row->render);
    free(row->hl);
}

void editor_row_ensure_flat(editor_row *row) {
    if (!row->str_dirty) return;
    free(row->str);
    row->str = malloc(row->size + 1);
    pt_copy_range(&editor.text, row->off, row->size, row->str);
    row->str[row->size] = '\0';
    row->str_dirty = 0;
}

static void editor_insert_row_meta(int at, int off, int size) {
    if (at < 0 || at > editor.rows) return;
    editor.row = realloc(editor.row, sizeof(editor_row) * (editor.rows + 1));
    memmove(&editor.row[at + 1], &editor.row[at],
            sizeof(editor_row) * (editor.rows - at));

    editor.row[at].idx = at;
    editor.row[at].off = off;
    editor.row[at].size = size;
    editor.row[at].render_size = 0;
    editor.row[at].str = NULL;
    editor.row[at].render = NULL;
    editor.row[at].hl = NULL;
    editor.row[at].hl_open_comment = 0;
    editor.row[at].str_dirty = 1;

    for (int j = at + 1; j <= editor.rows; j++)
        editor.row[j].idx = j;

    editor.rows++;
}

static void editor_del_row_meta(int at) {
    if (at < 0 || at >= editor.rows) return;
    editor_row_cache_free(&editor.row[at]);
    memmove(&editor.row[at], &editor.row[at + 1],
            sizeof(editor_row) * (editor.rows - at - 1));
    editor.rows--;
    for (int j = at; j < editor.rows; j++)
        editor.row[j].idx = j;
}

static void editor_shift_offsets_from(int from, int delta) {
    for (int i = from; i < editor.rows; i++)
        editor.row[i].off += delta;
}

int convert_row_to_render_x(editor_row *row, int x) {
    editor_row_ensure_flat(row);
    int render_x = 0;
    for (int j = 0; j < x; j++) {
        if (row->str[j] == '\t')
            render_x += (config.tab_size - 1) - (render_x % config.tab_size);
        render_x++;
    }
    return render_x;
}

int editor_render_x_to_cx(editor_row *row, int rx) {
    editor_row_ensure_flat(row);
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->str[cx] == '\t')
            cur_rx += (config.tab_size - 1) - (cur_rx % config.tab_size);
        cur_rx++;
        if (cur_rx > rx) return cx;
    }
    return cx;
}

char *editor_prompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        editor_set_status_message(prompt, buf);
        editor_refresh_screen();

        int c = editor_read_key();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            editor_set_status_message("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                editor_set_status_message("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback) callback(buf, c);
    }
}

static void editor_find_next(char *query, int qlen, int *last_row, int *last_col, int direction,
                             int *hl_line, unsigned char **hl_save) {
    if (qlen <= 0 || editor.rows == 0) return;

    /* Restore previous highlight */
    if (*hl_save) {
        memcpy(editor.row[*hl_line].hl, *hl_save,
               editor.row[*hl_line].render_size);
        free(*hl_save);
        *hl_save = NULL;
        *hl_line = -1;
    }

    int start_row = *last_row;
    int start_col = *last_col;

    /* Advance past current match so we don't find the same one again */
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

        /* Determine search range within this row */
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

                    /* Highlight the match on the render string */
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
            /* Cancel: restore position and highlight */
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
            /* Next match */
            editor_find_next(buf, (int)buflen, &last_row, &last_col, 1,
                             &hl_line, &hl_save);
        } else if (c == BACKSPACE || c == CTRL_KEY('h') || c == ARROW_LEFT || c == ARROW_UP) {
            /* Previous match */
            editor_find_next(buf, (int)buflen, &last_row, &last_col, -1,
                             &hl_line, &hl_save);
        } else if (c == DEL_KEY) {
            /* Delete last char from query */
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
            /* Append to query and search from beginning */
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
    int saved_cx = editor.cpos.x;
    int saved_cy = editor.cpos.y;
    int saved_coloff = editor.col_offset;
    int saved_rowoff = editor.row_offset;

    char *query = editor_prompt("Find: %s (ESC to cancel)", NULL);
    if (!query) {
        editor.cpos.x = saved_cx;
        editor.cpos.y = saved_cy;
        editor.col_offset = saved_coloff;
        editor.row_offset = saved_rowoff;
        return;
    }

    char *replacement = editor_prompt("Replace with: %s (ESC to cancel)", NULL);
    if (!replacement) {
        free(query);
        editor.cpos.x = saved_cx;
        editor.cpos.y = saved_cy;
        editor.col_offset = saved_coloff;
        editor.row_offset = saved_rowoff;
        return;
    }

    int qlen = (int)strlen(query);
    int rlen = (int)strlen(replacement);
    int replaced = 0;

    for (int r = 0; r < editor.rows; r++) {
        editor_row_ensure_flat(&editor.row[r]);
        int col = 0;
        while (col + qlen <= editor.row[r].size) {
            editor_row_ensure_flat(&editor.row[r]);
            if (memcmp(&editor.row[r].str[col], query, qlen) != 0) {
                col++;
                continue;
            }

            int off = editor.row[r].off + col;
            pt_delete(&editor.text, off, qlen);
            if (rlen > 0)
                pt_insert(&editor.text, off, replacement, rlen);

            int delta = rlen - qlen;
            editor.row[r].size += delta;
            editor_row_cache_invalidate(&editor.row[r]);
            editor_shift_offsets_from(r + 1, delta);

            /* Check if replacement introduced newlines */
            int has_nl = 0;
            for (int k = 0; k < rlen; k++) {
                if (replacement[k] == '\n') { has_nl = 1; break; }
            }
            if (has_nl) {
                /* Rebuild row metadata from scratch for this region */
                /* For simplicity, rescan from row r */
                editor_row_ensure_flat(&editor.row[r]);
                /* Find newlines in this row's content */
                char *s = editor.row[r].str;
                int sz = editor.row[r].size;
                int first_nl = -1;
                for (int k = 0; k < sz; k++) {
                    if (s[k] == '\n') { first_nl = k; break; }
                }
                if (first_nl >= 0) {
                    int orig_off = editor.row[r].off;
                    int orig_size = editor.row[r].size;
                    /* Shrink current row to before first newline */
                    editor.row[r].size = first_nl;
                    editor_row_cache_invalidate(&editor.row[r]);
                    /* Insert new rows for each subsequent line */
                    int scan = first_nl + 1;
                    int insert_at = r + 1;
                    while (scan <= orig_size) {
                        int line_start = scan;
                        while (scan < orig_size && s[scan] != '\n') scan++;
                        int line_len = scan - line_start;
                        editor_insert_row_meta(insert_at, orig_off + line_start, line_len);
                        editor_update_row(&editor.row[insert_at]);
                        insert_at++;
                        scan++; /* skip newline */
                    }
                }
            }

            editor_update_row(&editor.row[r]);
            replaced++;
            col += rlen;
        }
    }

    free(query);
    free(replacement);

    if (replaced > 0) {
        editor.dirty++;
        editor_set_status_message("Replaced %d occurrence(s).", replaced);
    } else {
        editor_set_status_message("No matches found.");
    }
}

void editor_set_status_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(editor.status, sizeof(editor.status), fmt, ap);
    va_end(ap);
    editor.status_time = time(NULL);
}

void editor_update_row(editor_row *row) {
    editor_row_ensure_flat(row);
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
        if (row->str[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs * (config.tab_size - 1) + 1);

    int i = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->str[j] == '\t') {
            row->render[i++] = ' ';
            while (i % config.tab_size != 0) row->render[i++] = ' ';
        } else {
            row->render[i++] = row->str[j];
        }
    }
    row->render[i] = '\0';
    row->render_size = i;

    editor_update_syntax(row);
}

void editor_insert_char(int c) {
    if (editor.cpos.y == editor.rows) {
        if (editor.rows == 0) {
            editor_insert_row_meta(0, 0, 0);
        } else {
            int old_len = editor.text.len;
            pt_insert(&editor.text, old_len, "\n", 1);
            editor_insert_row_meta(editor.rows, old_len + 1, 0);
        }
    }

    editor_row *row = &editor.row[editor.cpos.y];
    int off = row->off + editor.cpos.x;
    char ch = (char)c;

    pt_insert(&editor.text, off, &ch, 1);
    row->size++;
    editor_shift_offsets_from(editor.cpos.y + 1, +1);

    editor_row_cache_invalidate(row);
    editor_update_row(row);

    editor.cpos.x++;
    editor.dirty++;
}

void editor_insert_newline(void) {
    if (editor.cpos.y == editor.rows) {
        if (editor.rows == 0) {
            editor_insert_row_meta(0, 0, 0);
        } else {
            int old_len = editor.text.len;
            pt_insert(&editor.text, old_len, "\n", 1);
            editor_insert_row_meta(editor.rows, old_len + 1, 0);
        }
        editor.cpos.y++;
        editor.cpos.x = 0;
        editor.dirty++;
        return;
    }

    editor_row *row = &editor.row[editor.cpos.y];
    int old_size = row->size;
    int split_off = row->off + editor.cpos.x;

    pt_insert(&editor.text, split_off, "\n", 1);

    row->size = editor.cpos.x;
    editor_row_cache_invalidate(row);

    editor_insert_row_meta(editor.cpos.y + 1, split_off + 1, old_size - editor.cpos.x);
    editor_shift_offsets_from(editor.cpos.y + 2, +1);

    editor_update_row(&editor.row[editor.cpos.y]);
    editor_update_row(&editor.row[editor.cpos.y + 1]);

    editor.cpos.y++;
    editor.cpos.x = 0;
    editor.dirty++;
}

void editor_del_char(void) {
    if (editor.cpos.y == editor.rows) return;
    if (editor.cpos.x == 0 && editor.cpos.y == 0) return;

    if (editor.cpos.x > 0) {
        editor_row *row = &editor.row[editor.cpos.y];
        int off = row->off + editor.cpos.x - 1;

        pt_delete(&editor.text, off, 1);
        row->size--;
        editor_shift_offsets_from(editor.cpos.y + 1, -1);

        editor_row_cache_invalidate(row);
        editor_update_row(row);

        editor.cpos.x--;
        editor.dirty++;
    } else {
        editor_row *prev = &editor.row[editor.cpos.y - 1];
        editor_row *cur  = &editor.row[editor.cpos.y];

        int prev_size = prev->size;
        int nl_off = prev->off + prev->size;

        pt_delete(&editor.text, nl_off, 1);

        prev->size += cur->size;
        editor_row_cache_invalidate(prev);

        editor_del_row_meta(editor.cpos.y);
        editor_shift_offsets_from(editor.cpos.y, -1);

        editor_update_row(prev);

        editor.cpos.y--;
        editor.cpos.x = prev_size;
        editor.dirty++;
    }
}

char *editor_rows_to_string(int *buflen) {
    int len;
    char *buf = pt_flatten(&editor.text, &len);
    /* Ensure file ends with a trailing newline */
    char *out = malloc(len + 2);
    memcpy(out, buf, len);
    out[len] = '\n';
    out[len + 1] = '\0';
    free(buf);
    *buflen = len + 1;
    return out;
}

void editor_save(void) {
    if (file_name == NULL) {
        char *name = editor_prompt("Save as: %s (ESC to cancel)", NULL);
        if (!name || name[0] == '\0') {
            if (name) free(name);
            editor_set_status_message("Save aborted.");
            return;
        }
        file_name = name;
        editor_select_syntax_highlight();
    }

    int len;
    char *buf = editor_rows_to_string(&len);

    int fd = open(file_name, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                editor.dirty = 0;
                editor_set_status_message("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}

void editor_scroll(void) {
    editor.render_x = editor.cpos.x;
    if (editor.cpos.y < editor.rows)
        editor.render_x = convert_row_to_render_x(&editor.row[editor.cpos.y], editor.cpos.x);

    if (editor.cpos.y < editor.row_offset)
        editor.row_offset = editor.cpos.y;
    if (editor.cpos.y >= editor.row_offset + editor.win_size.rows)
        editor.row_offset = editor.cpos.y - editor.win_size.rows + 1;
    if (editor.render_x < editor.col_offset)
        editor.col_offset = editor.render_x;
    if (editor.render_x >= editor.col_offset + editor.win_size.cols)
        editor.col_offset = editor.render_x - editor.win_size.cols + 1;
}

void editor_draw_rows(struct abuf *ab) {
    if (!EKO_COLOR_IS_NONE(config.color_bg)) {
        char buf[32];
        int len = eko_color_bg(config.color_bg, buf, sizeof(buf));
        abuf_append(ab, buf, len);
    }

    for (int y = 0; y < editor.win_size.rows; y++) {
        int filerow = y + editor.row_offset;
        if (filerow >= editor.rows) {
            if (editor.rows == 0 && y == editor.win_size.rows / 3) {
                char welcome[sizeof(EKO_WELCOME)];
                int welcomelen = snprintf(welcome, sizeof(welcome), "%s", EKO_WELCOME);
                if (welcomelen > editor.win_size.cols) welcomelen = editor.win_size.cols;
                int padding = (editor.win_size.cols - welcomelen) / 2;
                if (padding) { abuf_append(ab, "~", 1); padding--; }
                while (padding--) abuf_append(ab, " ", 1);
                abuf_append(ab, welcome, welcomelen);
            } else {
                abuf_append(ab, "~", 1);
            }
        } else {
            int len = editor.row[filerow].render_size - editor.col_offset;
            if (len < 0) len = 0;
            if (len > editor.win_size.cols) len = editor.win_size.cols;

            char *c = &editor.row[filerow].render[editor.col_offset];
            unsigned char *hl = editor.row[filerow].hl
                                ? &editor.row[filerow].hl[editor.col_offset]
                                : NULL;
            int current_color = EKO_COLOR_NONE;

            for (int j = 0; j < len; j++) {
                if (iscntrl((unsigned char)c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abuf_append(ab, "\x1b[7m", 4);
                    abuf_append(ab, &sym, 1);
                    abuf_append(ab, "\x1b[m", 3);
                    if (!EKO_COLOR_IS_NONE(config.color_bg)) {
                        char buf[32];
                        int blen = eko_color_bg(config.color_bg, buf, sizeof(buf));
                        abuf_append(ab, buf, blen);
                    }
                    if (!EKO_COLOR_IS_NONE(current_color)) {
                        char buf[32];
                        int clen = eko_color_fg(current_color, buf, sizeof(buf));
                        abuf_append(ab, buf, clen);
                    }
                } else if (!hl || hl[j] == HL_NORMAL) {
                    if (!EKO_COLOR_IS_NONE(current_color)) {
                        abuf_append(ab, "\x1b[39m", 5);
                        current_color = EKO_COLOR_NONE;
                    }
                    abuf_append(ab, &c[j], 1);
                } else {
                    int color = editor_syntax_to_color(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[32];
                        int clen = eko_color_fg(color, buf, sizeof(buf));
                        abuf_append(ab, buf, clen);
                    }
                    abuf_append(ab, &c[j], 1);
                }
            }
            abuf_append(ab, "\x1b[39m", 5);
        }

        abuf_append(ab, "\x1b[K", 3);
        abuf_append(ab, "\r\n", 2);
    }

    if (!EKO_COLOR_IS_NONE(config.color_bg))
        abuf_append(ab, "\x1b[49m", 5);
}

void editor_draw_status_bar(struct abuf *ab) {
    abuf_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s%s - %d lines",
        file_name ? file_name : "[No Name]",
        editor.dirty ? " (modified)" : "",
        editor.rows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
        editor.syntax ? editor.syntax->filetype : "no ft",
        editor.cpos.y + 1, editor.rows);

    if (len > editor.win_size.cols) len = editor.win_size.cols;
    abuf_append(ab, status, len);

    while (len < editor.win_size.cols) {
        if (editor.win_size.cols - len == rlen) {
            abuf_append(ab, rstatus, rlen);
            break;
        }
        abuf_append(ab, " ", 1);
        len++;
    }

    abuf_append(ab, "\x1b[m", 3);
    abuf_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(struct abuf *ab) {
    abuf_append(ab, "\x1b[K", 3);
    int len = strlen(editor.status);
    if (len > editor.win_size.cols) len = editor.win_size.cols;
    if (len && time(NULL) - editor.status_time < 5)
        abuf_append(ab, editor.status, len);
}

void editor_refresh_screen(void) {
    editor_scroll();

    struct abuf ab = ABUF_INIT;
    abuf_append(&ab, "\x1b[?25l", 6);
    abuf_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);
    editor_draw_status_bar(&ab);
    editor_draw_message_bar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
             (editor.cpos.y - editor.row_offset) + 1,
             (editor.render_x - editor.col_offset) + 1);
    abuf_append(&ab, buf, strlen(buf));
    abuf_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    abuf_free(&ab);
}

int editor_read_key(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }

    return c;
}

void editor_move_cursor(int key) {
    editor_row *row = (editor.cpos.y >= editor.rows) ? NULL : &editor.row[editor.cpos.y];

    switch (key) {
        case ARROW_LEFT:
            if (editor.cpos.x != 0) {
                editor.cpos.x--;
            } else if (editor.cpos.y > 0) {
                editor.cpos.y--;
                editor.cpos.x = editor.row[editor.cpos.y].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && editor.cpos.x < row->size) {
                editor.cpos.x++;
            } else if (row && editor.cpos.x == row->size) {
                editor.cpos.y++;
                editor.cpos.x = 0;
            }
            break;
        case ARROW_UP:
            if (editor.cpos.y != 0) editor.cpos.y--;
            break;
        case ARROW_DOWN:
            if (editor.cpos.y < editor.rows) editor.cpos.y++;
            break;
    }

    row = (editor.cpos.y >= editor.rows) ? NULL : &editor.row[editor.cpos.y];
    int len = row ? row->size : 0;
    if (editor.cpos.x > len)
        editor.cpos.x = len;
}

void editor_process_keypress(void) {
    static int quit_times = 2;
    int c = editor_read_key();

    switch (c) {
        case '\r':
            editor_insert_newline();
            break;

        case CTRL_KEY('q'):
            if (editor.dirty && quit_times > 0) {
                editor_set_status_message(
                    "WARNING: Unsaved changes. Press Ctrl-Q %d more time(s) to quit.",
                    quit_times);
                quit_times--;
                return;
            }
            clear_screen();
            exit(0);
            break;

        case CTRL_KEY('s'):
            editor_save();
            break;

        case CTRL_KEY('f'):
            editor_find();
            break;

        case CTRL_KEY('r'):
            editor_find_replace();
            break;

        case CTRL_KEY('o'):
            editor_switch_file();
            break;

        case HOME_KEY:
            editor.cpos.x = 0;
            break;
        case END_KEY:
            if (editor.cpos.y < editor.rows)
                editor.cpos.x = editor.row[editor.cpos.y].size;
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
            editor_del_char();
            break;
        case DEL_KEY:
            editor_move_cursor(ARROW_RIGHT);
            editor_del_char();
            break;

        case ARROW_UP:
        case ARROW_RIGHT:
        case ARROW_DOWN:
        case ARROW_LEFT:
            editor_move_cursor(c);
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                editor.cpos.y = editor.row_offset;
            } else {
                editor.cpos.y = editor.row_offset + editor.win_size.rows - 1;
                if (editor.cpos.y > editor.rows) editor.cpos.y = editor.rows;
            }
            int t = editor.win_size.rows;
            while (t--) editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        }

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        case '\t':
            if (config.expand_tabs) {
                int spaces = config.tab_size - (editor.cpos.x % config.tab_size);
                for (int i = 0; i < spaces; i++) editor_insert_char(' ');
            } else {
                editor_insert_char('\t');
            }
            break;

        default:
            if (c >= 32 && c < 127) editor_insert_char(c);
            break;
    }

    quit_times = 2;
}

static void editor_free_content(void) {
    for (int i = 0; i < editor.rows; i++) {
        free(editor.row[i].str);
        free(editor.row[i].render);
        free(editor.row[i].hl);
    }
    free(editor.row);
    editor.row = NULL;
    editor.rows = 0;
    pt_free(&editor.text);
    editor.cpos.x = 0;
    editor.cpos.y = 0;
    editor.row_offset = 0;
    editor.col_offset = 0;
    editor.render_x = 0;
    editor.dirty = 0;
    editor.syntax = NULL;
}

void editor_switch_file(void) {
    if (editor.dirty) {
        char *resp = editor_prompt(
            "Unsaved changes! Save first? (y/n): %s", NULL);
        if (resp) {
            if (resp[0] == 'y' || resp[0] == 'Y')
                editor_save();
            free(resp);
        } else {
            editor_set_status_message("Switch cancelled.");
            return;
        }
    }

    char *name = editor_prompt("Open file: %s (ESC to cancel)", NULL);
    if (!name || name[0] == '\0') {
        if (name) free(name);
        editor_set_status_message("Switch cancelled.");
        return;
    }

    /* Check file exists before switching */
    FILE *test = fopen(name, "r");
    if (!test) {
        /* New file — just switch to empty buffer */
        editor_free_content();
        free(file_name);
        file_name = name;
        editor_select_syntax_highlight();
        editor_set_status_message("New file: %s", file_name);
        return;
    }
    fclose(test);

    editor_free_content();
    free(file_name);
    file_name = name;
    editor_open();
    editor_set_status_message("Opened %s - %d lines", file_name, editor.rows);
}

void editor_open(void) {
    FILE *fp = fopen(file_name, "r");
    if (!fp) die("fopen");

    char *filebuf = NULL;
    int filelen = 0;
    int filecap = 0;

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, fp)) != -1) {
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            len--;

        int off = filelen;
        if (editor.rows > 0) {
            /* append newline separator */
            if (filelen >= filecap) {
                filecap = filecap ? filecap * 2 : 1024;
                filebuf = realloc(filebuf, filecap);
            }
            filebuf[filelen++] = '\n';
            off = filelen;
        }

        if (filelen + (int)len > filecap) {
            while (filecap < filelen + (int)len)
                filecap = filecap ? filecap * 2 : 1024;
            filebuf = realloc(filebuf, filecap);
        }
        if (len > 0) {
            memcpy(&filebuf[filelen], line, len);
            filelen += (int)len;
        }

        editor_insert_row_meta(editor.rows, off, (int)len);
    }

    pt_init(&editor.text, filebuf ? filebuf : "", filelen);
    free(filebuf);
    free(line);
    fclose(fp);

    for (int i = 0; i < editor.rows; i++)
        editor_update_row(&editor.row[i]);

    editor.dirty = 0;
    editor_select_syntax_highlight();
}

void editor_init(void) {
    editor.cpos.x    = 0;
    editor.cpos.y    = 0;
    editor.row       = NULL;
    editor.rows      = 0;
    editor.dirty     = 0;
    editor.syntax    = NULL;
    editor.row_offset = 0;
    editor.col_offset = 0;
    editor.render_x  = 0;
    editor.status[0] = '\0';
    editor.status_time = 0;

    memset(&editor.text, 0, sizeof(editor.text));

    if (get_window_size(&editor.win_size.rows, &editor.win_size.cols) == -1)
        die("get_window_size");
    editor.win_size.rows -= 2;
}
