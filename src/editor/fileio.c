#include "editor.h"

struct editor_config editor;
char *file_name;

char *editor_rows_to_string(int *buflen) {
    int len;
    char *buf = pt_flatten(&editor.text, &len);
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

    FILE *test = fopen(name, "r");
    if (!test) {
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

    editor.line_num_width = 3;

    editor.selecting     = 0;
    editor.mark_x        = 0;
    editor.mark_y        = 0;
    editor.clipboard     = NULL;
    editor.clipboard_len = 0;

    memset(&editor.text, 0, sizeof(editor.text));

    if (get_window_size(&editor.win_size.rows, &editor.win_size.cols) == -1)
        die("get_window_size");
    editor.win_size.rows -= 2;
}
