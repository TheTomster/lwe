LIBS = -lcurses
# bsd-compat may be needed on non-bsd systems, if you get link errors
# for `flock`.
#LIBS += -lbsd-compat

CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -Os -D_DEFAULT_SOURCE
LDFLAGS += -g ${LIBS}

# CC = cc
