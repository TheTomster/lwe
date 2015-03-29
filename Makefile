LDLIBS = -lcurses

OBJS = src/llwe.o src/err.o src/buffer.o src/draw.o

llwe: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe $(OBJS) $(LDLIBS)

src/buffer.o: src/err.h
src/llwe.o: src/buffer.h src/err.h src/draw.h
