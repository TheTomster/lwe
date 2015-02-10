LDLIBS=-lcurses

llwe: src/llwe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe src/llwe.c $(LDLIBS)
