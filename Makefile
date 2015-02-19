LDLIBS=-lcurses

llwe: src/llwe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe src/llwe.c $(LDLIBS)

debug: CFLAGS+=-O0
debug: llwe

.PHONY: debug
