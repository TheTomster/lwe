LDLIBS = -lcurses

OBJS = src/llwe.o src/err.o src/buffer.o

llwe: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe $(OBJS) $(LDLIBS)
