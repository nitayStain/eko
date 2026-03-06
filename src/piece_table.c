#include <stdlib.h>
#include <string.h>
#include "piece_table.h"

static void pt_merge_adjacent(piece_table *pt) {
    int w = 0;
    for (int r = 0; r < pt->piece_count; r++) {
        if (pt->pieces[r].len == 0) continue;
        if (w > 0 &&
            pt->pieces[w - 1].src == pt->pieces[r].src &&
            pt->pieces[w - 1].off + pt->pieces[w - 1].len == pt->pieces[r].off) {
            pt->pieces[w - 1].len += pt->pieces[r].len;
        } else {
            pt->pieces[w++] = pt->pieces[r];
        }
    }
    pt->piece_count = w;
}

static void pt_ensure_piece_cap(piece_table *pt, int needed) {
    if (pt->piece_cap >= needed) return;
    int cap = pt->piece_cap ? pt->piece_cap : 4;
    while (cap < needed) cap *= 2;
    pt->pieces = realloc(pt->pieces, sizeof(pt_piece) * cap);
    pt->piece_cap = cap;
}

static void pt_ensure_add_cap(piece_table *pt, int needed) {
    if (pt->add_cap >= needed) return;
    int cap = pt->add_cap ? pt->add_cap : 64;
    while (cap < needed) cap *= 2;
    pt->add = realloc(pt->add, cap);
    pt->add_cap = cap;
}

void pt_init(piece_table *pt, const char *s, int len) {
    pt->orig = NULL;
    pt->orig_len = 0;
    pt->add = NULL;
    pt->add_len = 0;
    pt->add_cap = 0;
    pt->pieces = NULL;
    pt->piece_count = 0;
    pt->piece_cap = 0;
    pt->len = len;

    if (len > 0) {
        pt->orig = malloc(len);
        memcpy(pt->orig, s, len);
        pt->orig_len = len;

        pt_ensure_piece_cap(pt, 1);
        pt->pieces[0].src = PT_ORIG;
        pt->pieces[0].off = 0;
        pt->pieces[0].len = len;
        pt->piece_count = 1;
    }
}

void pt_free(piece_table *pt) {
    free(pt->orig);
    free(pt->add);
    free(pt->pieces);
    pt->orig = NULL;
    pt->add = NULL;
    pt->pieces = NULL;
    pt->orig_len = 0;
    pt->add_len = 0;
    pt->add_cap = 0;
    pt->piece_count = 0;
    pt->piece_cap = 0;
    pt->len = 0;
}

void pt_insert(piece_table *pt, int at, const char *s, int len) {
    if (at < 0) at = 0;
    if (at > pt->len) at = pt->len;
    if (len <= 0) return;

    int add_off = pt->add_len;
    pt_ensure_add_cap(pt, pt->add_len + len);
    memcpy(&pt->add[add_off], s, len);
    pt->add_len += len;

    pt_piece new_piece;
    new_piece.src = PT_ADD;
    new_piece.off = add_off;
    new_piece.len = len;

    if (pt->piece_count == 0 || at == pt->len) {
        pt_ensure_piece_cap(pt, pt->piece_count + 1);
        pt->pieces[pt->piece_count++] = new_piece;
    } else {
        int logical = 0;
        for (int i = 0; i < pt->piece_count; i++) {
            pt_piece *p = &pt->pieces[i];
            if (at >= logical && at < logical + p->len) {
                int split = at - logical;
                if (split == 0) {
                    pt_ensure_piece_cap(pt, pt->piece_count + 1);
                    memmove(&pt->pieces[i + 1], &pt->pieces[i],
                            sizeof(pt_piece) * (pt->piece_count - i));
                    pt->pieces[i] = new_piece;
                    pt->piece_count++;
                } else {
                    pt_piece tail;
                    tail.src = p->src;
                    tail.off = p->off + split;
                    tail.len = p->len - split;
                    p->len = split;

                    pt_ensure_piece_cap(pt, pt->piece_count + 2);
                    p = &pt->pieces[i];
                    memmove(&pt->pieces[i + 3], &pt->pieces[i + 1],
                            sizeof(pt_piece) * (pt->piece_count - i - 1));
                    pt->pieces[i + 1] = new_piece;
                    pt->pieces[i + 2] = tail;
                    pt->piece_count += 2;
                }
                break;
            }
            logical += p->len;
        }
    }

    pt->len += len;

    pt_merge_adjacent(pt);
}

void pt_delete(piece_table *pt, int at, int len) {
    if (at < 0 || at >= pt->len || len <= 0) return;
    if (at + len > pt->len) len = pt->len - at;

    int del_start = at;
    int del_end = at + len;
    int logical = 0;

    for (int i = 0; i < pt->piece_count && del_start < del_end; i++) {
        pt_piece *p = &pt->pieces[i];
        int piece_start = logical;
        int piece_end = logical + p->len;

        if (del_start >= piece_end) {
            logical = piece_end;
            continue;
        }
        if (del_end <= piece_start) break;

        int trim_start = (del_start > piece_start) ? del_start - piece_start : 0;
        int trim_end = (del_end < piece_end) ? piece_end - del_end : 0;

        if (trim_start == 0 && trim_end == 0) {
            p->len = 0;
        } else if (trim_start > 0 && trim_end > 0) {
            pt_piece tail;
            tail.src = p->src;
            tail.off = p->off + p->len - trim_end;
            tail.len = trim_end;
            p->len = trim_start;

            pt_ensure_piece_cap(pt, pt->piece_count + 1);
            p = &pt->pieces[i];
            memmove(&pt->pieces[i + 2], &pt->pieces[i + 1],
                    sizeof(pt_piece) * (pt->piece_count - i - 1));
            pt->pieces[i + 1] = tail;
            pt->piece_count++;
            i++;
        } else if (trim_start > 0) {
            p->len = trim_start;
        } else {
            p->off += p->len - trim_end;
            p->len = trim_end;
        }

        logical = piece_end;
    }

    pt->len -= len;

    pt_merge_adjacent(pt);
}

void pt_copy_range(const piece_table *pt, int at, int len, char *dst) {
    if (len <= 0) return;
    if (at < 0) at = 0;
    if (at + len > pt->len) len = pt->len - at;

    int logical = 0;
    int written = 0;

    for (int i = 0; i < pt->piece_count && written < len; i++) {
        pt_piece *p = &((piece_table *)pt)->pieces[i];
        int piece_start = logical;
        int piece_end = logical + p->len;

        if (at + written >= piece_end) {
            logical = piece_end;
            continue;
        }

        const char *src = (p->src == PT_ORIG) ? pt->orig : pt->add;
        int copy_start = (at + written > piece_start) ? at + written - piece_start : 0;
        int copy_end = (at + len < piece_end) ? at + len - piece_start : p->len;
        int copy_len = copy_end - copy_start;

        if (copy_len > 0) {
            memcpy(&dst[written], &src[p->off + copy_start], copy_len);
            written += copy_len;
        }

        logical = piece_end;
    }
}

char *pt_flatten(const piece_table *pt, int *len_out) {
    char *buf = malloc(pt->len + 1);
    int pos = 0;
    for (int i = 0; i < pt->piece_count; i++) {
        pt_piece *p = &((piece_table *)pt)->pieces[i];
        const char *src = (p->src == PT_ORIG) ? pt->orig : pt->add;
        memcpy(&buf[pos], &src[p->off], p->len);
        pos += p->len;
    }
    buf[pos] = '\0';
    if (len_out) *len_out = pt->len;
    return buf;
}
