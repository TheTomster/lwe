CFLAGS=-Wall -Wextra -std=c99 -g
LDFLAGS=-lcurses
llwe:
llwe: llwe.c
	$(CC) -o llwe llwe.c $(CFLAGS) $(LDFLAGS)

.PHONY: release
release: CFLAGS=-Wall -Wextra -std=c99 -Os -s
release: llwe

.PHONY: indent
indent: llwe.c
	indent -kr -i8 -il0 llwe.c
