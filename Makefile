CFLAGS=-Wall -Wextra -std=c99
llwe: CFLAGS+=-Os
llwe: llwe.c defines.h
	$(CC) $(CFLAGS) llwe.c -o llwe

debug: CFLAGS+=-g
debug: llwe
