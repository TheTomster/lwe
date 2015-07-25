CFLAGS = -O0 -g -Wall -Wextra -pedantic -Werror -std=c99 -pipe
LDLIBS = -lcurses

OBJS = lwe.o err.o buffer.o draw.o yank.o bang.o undo.o insert.o

all: lwe

clean:
	@rm -f *.o lwe

.c.o:
	@echo CC $<
	@$(CC) -c $(CFLAGS) $<

lwe: $(OBJS)
	@echo LD $@
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

draw.o: buffer.h draw.h yank.h
buffer.o: err.h buffer.h
lwe.o: buffer.h err.h draw.h yank.h bang.h undo.h insert.h
yank.o: yank.h
bang.o: bang.h err.h
undo.o: undo.h buffer.h
insert.o: insert.h buffer.h draw.h undo.h

.PHONY: all clean
