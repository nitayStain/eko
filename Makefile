CC      = cc
CFLAGS  = -Wall -Wextra -pedantic -std=c99 -Iinclude
PREFIX  = /usr/local

SRCS = src/main.c src/editor.c src/terminal.c src/abuf.c src/syntax.c src/config.c src/piece_table.c
HDRS = $(wildcard include/*.h)

eko: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

install: eko
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 eko $(DESTDIR)$(PREFIX)/bin/eko

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/eko

clean:
	rm -f eko

.PHONY: clean install uninstall
