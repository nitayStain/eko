#ifndef PIECE_TABLE_H
#define PIECE_TABLE_H

#include "eko.h"

void pt_init(piece_table *pt, const char *s, int len);
void pt_free(piece_table *pt);
void pt_insert(piece_table *pt, int at, const char *s, int len);
void pt_delete(piece_table *pt, int at, int len);
void pt_copy_range(const piece_table *pt, int at, int len, char *dst);
char *pt_flatten(const piece_table *pt, int *len_out);

#endif
