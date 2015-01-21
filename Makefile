CFLAGS=-Wall -Wextra -std=c99 -g
llwe:
llwe: llwe.c
	$(CC) $(CFLAGS) llwe.c -o llwe

.PHONY: release
release: CFLAGS=-Wall -Wextra -std=c99 -Os -flto -s
release: llwe

.PHONY: indent
indent: llwe.c
	indent -kr -i8 -il0 llwe.c
