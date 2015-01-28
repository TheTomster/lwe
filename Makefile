CFLAGS=-Wall -Wextra -pedantic -std=c89 -g
LDLIBS=-lcurses
llwe:
llwe: llwe.c

.PHONY: release
release: CFLAGS=-Wall -Wextra -pedantic -std=c89 -O3 -s
release: llwe

.PHONY: indent
indent: llwe.c
	indent -kr -i8 -il0 llwe.c
