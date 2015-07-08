CFLAGS = -O0 -g -Wall -Wextra -pedantic -Werror -std=c99 -pipe
LDLIBS = -lcurses

OBJS = llwe.o err.o buffer.o draw.o yank.o bang.o undo.o

all: llwe

clean:
	@rm -f *.o llwe

.c.o:
	@echo CC $<
	@$(CC) -c $(CFLAGS) $<

llwe: $(OBJS)
	@echo LD $@
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

draw.o: buffer.h draw.h yank.h
buffer.o: err.h buffer.h
llwe.o: buffer.h err.h draw.h yank.h bang.h undo.h
yank.o: yank.h
bang.o: bang.h err.h
undo.o: undo.h buffer.h

.PHONY: all clean
