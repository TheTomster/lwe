/* (C) 2015 Tom Wright */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "undo.h"
#include "buffer.h"

enum action {
	INSERT,
	DELETE
};

struct step {
	enum action a;
	unsigned s;
	unsigned start;
	unsigned end;
	char *text;
};

static unsigned s; /* current step number */
static struct step *u, *uh; /* undo step struct list, and head of list */
static int usz, ua;
#if 0
static char *t; /* texts for undoes to reference */
static int tsz, ta;
#endif

static int checku(void);
static void undosingle(void);

/* Creates u if needed and ensures u has room for more items. */
static int checku()
{
	struct step *newu;
	if (!u) {
		if (!(u = malloc(sizeof(*u) * 8192)))
			return -1;
		ua = 8192;
	}
	if (usz == ua) {
		if (!(newu = realloc(u, ua * 2)))
			return -1;
		u = newu;
		ua *= 2;
	}
	return 0;
}

static void undosingle()
{
	char *st;
	unsigned tsz;
	st = getbufstart();
	switch (uh->a) {
	case INSERT:
		bufdelete(st + uh->start, st + uh->end);
		break;
	case DELETE:
		tsz = uh->end - uh->start;
		bufinsertstr(uh->text, uh->text + tsz, st + uh->start);
		break;
	}
	free(uh->text);
	uh->text = NULL;
	uh--;
}

int recinsert(char *start, char *end)
{
	assert(inbuf(start) && inbuf(end));
	if (checku() < 0)
		return -1;
	if (!uh)
		uh = u;
	else
		uh++;
	uh->a = INSERT;
	uh->s = s;
	uh->start = start - getbufstart();
	uh->end = end - getbufstart();
	uh->text = NULL;
	return 0;
}

int recdelete(char *start, char *end)
{
	assert(inbuf(start) && inbuf(end));
	if (checku() < 0)
		return -1;
	if (!uh)
		uh = u;
	else
		uh++;
	uh->a = DELETE;
	uh->s = s;
	uh->start = start - getbufstart();
	uh->end = end - getbufstart();
	if (!(uh->text = malloc(end - start))) {
		if (uh > u)
			uh--;
		return -1;
	}
	strncpy(uh->text, start, end - start);
	return 0;
}

void recstep()
{
	s++;
}

void undo()
{
	if (s > 0)
		s--;
	while (uh && uh >= u && uh->s >= s)
		undosingle();
}

void redo()
{}
