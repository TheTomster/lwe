CFLAGS=-Wall -Wextra -std=c99 -g
llwe:
llwe: llwe.c defines.h
	$(CC) $(CFLAGS) llwe.c -o llwe

release: CFLAGS=-Wall -Wextra -std=c99 -Os -flto -s
release: llwe
