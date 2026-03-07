#include <stdlib.h>
#include <string.h>
#include "abuf.h"

void abuf_append(struct abuf *ab, const char *s, int len) {
    if (ab->len + len > ab->cap) {
        int cap = ab->cap ? ab->cap : 256;
        while (cap < ab->len + len) cap *= 2;
        char *new = realloc(ab->buf, cap);
        if (!new) return;
        ab->buf = new;
        ab->cap = cap;
    }
    memcpy(&ab->buf[ab->len], s, len);
    ab->len += len;
}

void abuf_free(struct abuf *ab) {
    free(ab->buf);
}
