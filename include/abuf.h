#ifndef ABUF_H
#define ABUF_H

#define ABUF_INIT {NULL, 0, 0}

struct abuf {
    char *buf;
    int len;
    int cap;
};

void abuf_append(struct abuf *ab, const char *s, int len);
void abuf_free(struct abuf *ab);

#endif
