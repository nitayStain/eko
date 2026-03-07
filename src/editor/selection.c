#include "editor.h"

void editor_get_sel_offsets(int *start, int *end) {
    int a_off, b_off;
    if (editor.mark_y < editor.rows)
        a_off = editor.row[editor.mark_y].off + editor.mark_x;
    else
        a_off = editor.text.len;

    if (editor.cpos.y < editor.rows)
        b_off = editor.row[editor.cpos.y].off + editor.cpos.x;
    else
        b_off = editor.text.len;

    if (a_off <= b_off) {
        *start = a_off;
        *end   = b_off;
    } else {
        *start = b_off;
        *end   = a_off;
    }
}

void editor_select_start(void) {
    if (!editor.selecting) {
        editor.selecting = 1;
        editor.mark_x = editor.cpos.x;
        editor.mark_y = editor.cpos.y;
    }
}

void editor_select_clear(void) {
    editor.selecting = 0;
}

void editor_delete_selection(void) {
    int start, end;
    editor_get_sel_offsets(&start, &end);
    int len = end - start;
    if (len <= 0) { editor.selecting = 0; return; }

    pt_delete(&editor.text, start, len);
    editor_rebuild_rows();
    editor_offset_to_cursor(start, &editor.cpos.y, &editor.cpos.x);
    editor.selecting = 0;
    editor.dirty++;
}

void editor_copy(void) {
    if (!editor.selecting) return;

    int start, end;
    editor_get_sel_offsets(&start, &end);
    int len = end - start;
    if (len <= 0) return;

    free(editor.clipboard);
    editor.clipboard = malloc(len);
    editor.clipboard_len = len;
    pt_copy_range(&editor.text, start, len, editor.clipboard);

    FILE *pb = popen("pbcopy", "w");
    if (pb) {
        fwrite(editor.clipboard, 1, editor.clipboard_len, pb);
        pclose(pb);
    }

    editor_set_status_message("Copied %d bytes", len);
}

void editor_cut(void) {
    if (!editor.selecting) return;

    int start, end;
    editor_get_sel_offsets(&start, &end);
    int len = end - start;
    if (len <= 0) return;

    free(editor.clipboard);
    editor.clipboard = malloc(len);
    editor.clipboard_len = len;
    pt_copy_range(&editor.text, start, len, editor.clipboard);

    FILE *pb = popen("pbcopy", "w");
    if (pb) {
        fwrite(editor.clipboard, 1, editor.clipboard_len, pb);
        pclose(pb);
    }

    pt_delete(&editor.text, start, len);
    editor_rebuild_rows();
    editor_offset_to_cursor(start, &editor.cpos.y, &editor.cpos.x);
    editor.selecting = 0;
    editor.dirty++;
    editor_set_status_message("Cut %d bytes", len);
}

void editor_paste(void) {
    if (editor.selecting) editor_delete_selection();

    FILE *pb = popen("pbpaste", "r");
    if (pb) {
        char *buf = NULL;
        int buflen = 0, bufcap = 0;
        char tmp[1024];
        size_t n;
        while ((n = fread(tmp, 1, sizeof(tmp), pb)) > 0) {
            if (buflen + (int)n > bufcap) {
                bufcap = (buflen + (int)n) * 2;
                buf = realloc(buf, bufcap);
            }
            memcpy(&buf[buflen], tmp, n);
            buflen += (int)n;
        }
        pclose(pb);
        if (buf && buflen > 0) {
            free(editor.clipboard);
            editor.clipboard = buf;
            editor.clipboard_len = buflen;
        } else {
            free(buf);
        }
    }

    if (!editor.clipboard || editor.clipboard_len <= 0) return;

    if (editor.cpos.y == editor.rows) {
        if (editor.rows == 0) {
            editor_insert_row_meta(0, 0, 0);
        } else {
            int old_len = editor.text.len;
            pt_insert(&editor.text, old_len, "\n", 1);
            editor_insert_row_meta(editor.rows, old_len + 1, 0);
        }
    }

    int off = editor.row[editor.cpos.y].off + editor.cpos.x;
    pt_insert(&editor.text, off, editor.clipboard, editor.clipboard_len);
    editor_rebuild_rows();

    int target = off + editor.clipboard_len;
    editor_offset_to_cursor(target, &editor.cpos.y, &editor.cpos.x);

    editor.dirty++;
    editor_set_status_message("Pasted %d bytes", editor.clipboard_len);
}
