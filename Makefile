CFLAGS = -O0 -g -Wall -Wextra -pedantic -Werror -std=c99 -pipe
LDLIBS = -lcurses

OBJS = src/llwe.o src/err.o src/buffer.o src/draw.o src/yank.o src/bang.o

llwe: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o llwe $(OBJS) $(LDLIBS)

src/draw.o: src/buffer.h src/draw.h src/yank.h
src/buffer.o: src/err.h src/buffer.h
src/llwe.o: src/buffer.h src/err.h src/draw.h src/yank.h src/bang.h
src/yank.o: src/yank.h
src/bang.o: src/bang.h src/err.h
