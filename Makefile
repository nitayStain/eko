CC      = cc
CFLAGS  = -Wall -Wextra -pedantic -std=c99 -Iinclude
PREFIX  = /usr/local

SRCS = $(wildcard src/*.c) $(wildcard src/editor/*.c)
HDRS = $(wildcard include/*.h) $(wildcard include/editor/*.h)

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
