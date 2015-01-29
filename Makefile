CFLAGS ?= -Wall -Wextra -pedantic -std=c89 -O3 -g
LDLIBS = -lcurses

llwe: llwe.c

indent: llwe.c
	indent -kr -i8 -il0 $<

.PHONY: indent
