LDLIBS = -lcurses

OBJS = src/llwe.o src/err.o src/buffer.o src/draw.o

llwe: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe $(OBJS) $(LDLIBS)

src/draw.o: src/buffer.h src/draw.h
src/buffer.o: src/err.h src/buffer.h
src/llwe.o: src/buffer.h src/err.h src/draw.h
