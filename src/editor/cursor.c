#include "editor.h"

void editor_update_line_num_width(void) {
    if (!config.line_numbers) {
        editor.line_num_width = 0;
        return;
    }
    int n = editor.rows > 0 ? editor.rows : 1;
    int digits = 0;
    while (n > 0) { digits++; n /= 10; }
    if (digits < 2) digits = 2;
    editor.line_num_width = digits + 1;
}

void editor_scroll(void) {
    editor_update_line_num_width();

    editor.render_x = editor.cpos.x;
    if (editor.cpos.y < editor.rows)
        editor.render_x = convert_row_to_render_x(&editor.row[editor.cpos.y], editor.cpos.x);

    int text_cols = editor.win_size.cols - editor.line_num_width;

    if (editor.cpos.y < editor.row_offset)
        editor.row_offset = editor.cpos.y;
    if (editor.cpos.y >= editor.row_offset + editor.win_size.rows)
        editor.row_offset = editor.cpos.y - editor.win_size.rows + 1;
    if (editor.render_x < editor.col_offset)
        editor.col_offset = editor.render_x;
    if (editor.render_x >= editor.col_offset + text_cols)
        editor.col_offset = editor.render_x - text_cols + 1;
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
