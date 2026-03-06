#ifndef TERMINAL_H
#define TERMINAL_H

void clear_screen(void);
void die(const char *s);
void disable_raw_mode(void);
void enable_raw_mode(void);
int  get_cursor_position(int *rows, int *cols);
int  get_window_size(int *rows, int *cols);

#endif
