CFLAGS=-Wall -Wextra -std=c99
llwe: llwe.c defines.h
	$(CC) $(CFLAGS) llwe.c -o llwe

debug: CFLAGS+=-g
debug: llwe
