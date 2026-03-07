#include "editor.h"

void editor_draw_rows(struct abuf *ab) {
    if (!EKO_COLOR_IS_NONE(config.color_bg)) {
        char buf[32];
        int len = eko_color_bg(config.color_bg, buf, sizeof(buf));
        abuf_append(ab, buf, len);
    }

    int gutter = editor.line_num_width;
    int text_cols = editor.win_size.cols - gutter;

    for (int y = 0; y < editor.win_size.rows; y++) {
        int filerow = y + editor.row_offset;
        if (filerow >= editor.rows) {
            if (gutter > 0) {
                char gpad[16];
                int glen = snprintf(gpad, sizeof(gpad), "%*s", gutter, "");
                abuf_append(ab, "\x1b[90m", 5);
                abuf_append(ab, gpad, glen);
                abuf_append(ab, "\x1b[39m", 5);
            }

            if (editor.rows == 0 && y == editor.win_size.rows / 3) {
                char welcome[sizeof(EKO_WELCOME)];
                int welcomelen = snprintf(welcome, sizeof(welcome), "%s", EKO_WELCOME);
                if (welcomelen > text_cols) welcomelen = text_cols;
                int padding = (text_cols - welcomelen) / 2;
                while (padding--) abuf_append(ab, " ", 1);
                abuf_append(ab, welcome, welcomelen);
            } else {
                abuf_append(ab, "~", 1);
            }
        } else {
            if (gutter > 0) {
                char lnum[16];
                int llen = snprintf(lnum, sizeof(lnum), "%*d ", gutter - 1, filerow + 1);
                abuf_append(ab, "\x1b[90m", 5);
                abuf_append(ab, lnum, llen);
                abuf_append(ab, "\x1b[39m", 5);
            }

            int len = editor.row[filerow].render_size - editor.col_offset;
            if (len < 0) len = 0;
            if (len > text_cols) len = text_cols;

            char *c = &editor.row[filerow].render[editor.col_offset];
            unsigned char *hl = editor.row[filerow].hl
                                ? &editor.row[filerow].hl[editor.col_offset]
                                : NULL;
            int current_color = EKO_COLOR_NONE;

            int sel_start_rx = -1, sel_end_rx = -1;
            if (editor.selecting) {
                int s_off, e_off;
                editor_get_sel_offsets(&s_off, &e_off);
                int row_off = editor.row[filerow].off;
                int row_end = row_off + editor.row[filerow].size;
                if (s_off < row_end && e_off > row_off) {
                    int sc = (s_off > row_off) ? s_off - row_off : 0;
                    int ec = (e_off < row_end) ? e_off - row_off : editor.row[filerow].size;
                    sel_start_rx = convert_row_to_render_x(&editor.row[filerow], sc)
                                   - editor.col_offset;
                    sel_end_rx   = convert_row_to_render_x(&editor.row[filerow], ec)
                                   - editor.col_offset;
                    if (sel_start_rx < 0) sel_start_rx = 0;
                    if (sel_end_rx > len) sel_end_rx = len;
                }
            }

            int in_sel = 0;
            for (int j = 0; j < len; j++) {
                int want_sel = (sel_start_rx >= 0 && j >= sel_start_rx && j < sel_end_rx);
                if (want_sel && !in_sel) {
                    char sbuf[32];
                    int slen = eko_color_bg(config.color_selection, sbuf, sizeof(sbuf));
                    if (slen > 0) abuf_append(ab, sbuf, slen);
                    else abuf_append(ab, "\x1b[7m", 4);
                    in_sel = 1;
                } else if (!want_sel && in_sel) {
                    if (!EKO_COLOR_IS_NONE(config.color_bg)) {
                        char bbuf[32];
                        int blen = eko_color_bg(config.color_bg, bbuf, sizeof(bbuf));
                        abuf_append(ab, bbuf, blen);
                    } else {
                        abuf_append(ab, "\x1b[49m", 5);
                    }
                    in_sel = 0;
                }

                if (iscntrl((unsigned char)c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    if (!in_sel) abuf_append(ab, "\x1b[7m", 4);
                    abuf_append(ab, &sym, 1);
                    if (!in_sel) abuf_append(ab, "\x1b[m", 3);
                    if (!in_sel && !EKO_COLOR_IS_NONE(config.color_bg)) {
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
            if (in_sel) {
                if (!EKO_COLOR_IS_NONE(config.color_bg)) {
                    char bbuf[32];
                    int blen = eko_color_bg(config.color_bg, bbuf, sizeof(bbuf));
                    abuf_append(ab, bbuf, blen);
                } else {
                    abuf_append(ab, "\x1b[49m", 5);
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
             (editor.render_x - editor.col_offset) + 1 + editor.line_num_width);
    abuf_append(&ab, buf, strlen(buf));
    abuf_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    abuf_free(&ab);
}
