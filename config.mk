LIBS = -lcurses

CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -Os -D_XOPEN_SOURCE=700
LDFLAGS += -g ${LIBS}

# CC = cc
