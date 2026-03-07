#include "editor.h"

int editor_read_key(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[5];
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
                if (seq[1] == '1' && seq[2] == ';') {
                    char mod, dir;
                    if (read(STDIN_FILENO, &mod, 1) != 1) return '\x1b';
                    if (read(STDIN_FILENO, &dir, 1) != 1) return '\x1b';
                    if (mod == '2') {
                        switch (dir) {
                            case 'A': return SHIFT_ARROW_UP;
                            case 'B': return SHIFT_ARROW_DOWN;
                            case 'C': return SHIFT_ARROW_RIGHT;
                            case 'D': return SHIFT_ARROW_LEFT;
                        }
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

void editor_process_keypress(void) {
    static int quit_times = 2;
    int c = editor_read_key();

    switch (c) {
        case '\r':
            if (editor.selecting) editor_delete_selection();
            else editor_insert_newline();
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
            editor_select_clear();
            editor_find();
            break;

        case CTRL_KEY('r'):
            editor_select_clear();
            editor_find_replace();
            break;

        case CTRL_KEY('o'):
            editor_select_clear();
            editor_switch_file();
            break;

        case CTRL_KEY('g'):
            editor_select_clear();
            editor_goto_line();
            break;

        case CTRL_KEY('c'):
            editor_copy();
            editor_select_clear();
            break;

        case CTRL_KEY('x'):
            editor_cut();
            break;

        case CTRL_KEY('v'):
            editor_paste();
            break;

        case HOME_KEY:
            editor_select_clear();
            editor.cpos.x = 0;
            break;
        case END_KEY:
            editor_select_clear();
            if (editor.cpos.y < editor.rows)
                editor.cpos.x = editor.row[editor.cpos.y].size;
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
            if (editor.selecting) editor_delete_selection();
            else editor_del_char();
            break;
        case DEL_KEY:
            if (editor.selecting) {
                editor_delete_selection();
            } else {
                editor_move_cursor(ARROW_RIGHT);
                editor_del_char();
            }
            break;

        case SHIFT_ARROW_UP:
        case SHIFT_ARROW_DOWN:
        case SHIFT_ARROW_LEFT:
        case SHIFT_ARROW_RIGHT:
            editor_select_start();
            switch (c) {
                case SHIFT_ARROW_UP:    editor_move_cursor(ARROW_UP);    break;
                case SHIFT_ARROW_DOWN:  editor_move_cursor(ARROW_DOWN);  break;
                case SHIFT_ARROW_LEFT:  editor_move_cursor(ARROW_LEFT);  break;
                case SHIFT_ARROW_RIGHT: editor_move_cursor(ARROW_RIGHT); break;
            }
            break;

        case ARROW_UP:
        case ARROW_RIGHT:
        case ARROW_DOWN:
        case ARROW_LEFT:
            editor_select_clear();
            editor_move_cursor(c);
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            editor_select_clear();
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
            editor_select_clear();
            break;

        case '\t':
            if (editor.selecting) editor_delete_selection();
            if (config.expand_tabs) {
                int spaces = config.tab_size - (editor.cpos.x % config.tab_size);
                for (int i = 0; i < spaces; i++) editor_insert_char(' ');
            } else {
                editor_insert_char('\t');
            }
            break;

        default:
            if (c >= 32 && c < 127) {
                if (editor.selecting) editor_delete_selection();
                editor_insert_char(c);
            }
            break;
    }

    quit_times = 2;
}
