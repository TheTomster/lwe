CFLAGS=-Wall -Wextra -std=c99 -Os
llwe:
llwe: llwe.c defines.h
	$(CC) $(CFLAGS) llwe.c -o llwe

debug: CFLAGS=-Wall -Wextra -std=c99 -g
debug: llwe
