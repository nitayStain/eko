#include "editor.h"

void editor_row_cache_invalidate(editor_row *row) {
    free(row->str);
    row->str = NULL;
    free(row->render);
    row->render = NULL;
    free(row->hl);
    row->hl = NULL;
    row->render_size = 0;
    row->str_dirty = 1;
}

void editor_row_cache_free(editor_row *row) {
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

void editor_insert_row_meta(int at, int off, int size) {
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

void editor_del_row_meta(int at) {
    if (at < 0 || at >= editor.rows) return;
    editor_row_cache_free(&editor.row[at]);
    memmove(&editor.row[at], &editor.row[at + 1],
            sizeof(editor_row) * (editor.rows - at - 1));
    editor.rows--;
    for (int j = at; j < editor.rows; j++)
        editor.row[j].idx = j;
}

void editor_shift_offsets_from(int from, int delta) {
    for (int i = from; i < editor.rows; i++)
        editor.row[i].off += delta;
}

void editor_rebuild_rows(void) {
    for (int i = 0; i < editor.rows; i++) {
        free(editor.row[i].str);
        free(editor.row[i].render);
        free(editor.row[i].hl);
    }
    free(editor.row);
    editor.row = NULL;
    editor.rows = 0;

    int flat_len;
    char *flat = pt_flatten(&editor.text, &flat_len);
    int line_start = 0;
    for (int i = 0; i <= flat_len; i++) {
        if (i == flat_len || flat[i] == '\n') {
            editor_insert_row_meta(editor.rows, line_start, i - line_start);
            line_start = i + 1;
        }
    }
    free(flat);

    for (int i = 0; i < editor.rows; i++)
        editor_update_row(&editor.row[i]);
}

void editor_offset_to_cursor(int off, int *cy, int *cx) {
    *cy = 0;
    *cx = 0;
    int pos = 0;
    for (int i = 0; i < editor.rows; i++) {
        if (off >= pos && off <= pos + editor.row[i].size) {
            *cy = i;
            *cx = off - pos;
            return;
        }
        pos += editor.row[i].size + 1;
    }
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
