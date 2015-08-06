/* (C) 2015 Tom Wright */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "undo.h"
#include "buffer.h"

#define INIT_UNDO_SZ 128

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

static unsigned us, rs; /* current step number */
/* undo / redo step struct list, and head of list */
static struct step *u, *uh, *r, *rh;
static unsigned ua, ra; /* allocated size */

static int checkalloc(struct step **l, struct step **h, unsigned *a);
static int undosingle(void);
static int redosingle(void);
static void resetr(void);
static int storeins(struct step **l, struct step **h, unsigned *a,
                    unsigned s, char *start, char *end);
static int storedel(struct step **l, struct step **h, unsigned *a,
                    unsigned s, char *start, char *end);

/* Creates undo/redo list if needed, and ensures space for items. */
static int checkalloc(struct step **l, struct step **h, unsigned *a)
{
	struct step *new;
	if (!*l || *h == *l + *a - 1) {
		if (*a > 0) {
			*a *= 2;
		} else {
			*a = INIT_UNDO_SZ;
		}
		new = realloc(*l, *a * sizeof(**l));
		if (!new)
			return -1;
		if (*h)
			*h = new + (*h - *l);
		*l = new;
	}
	return 0;
}

static int undosingle()
{
	char *st;
	unsigned tsz;
	st = getbufstart();
	switch (uh->a) {
	case INSERT:
		if (storedel(&r, &rh, &ra, rs, st + uh->start, st + uh->end) < 0)
			return -1;
		bufdelete(st + uh->start, st + uh->end);
		break;
	case DELETE:
		tsz = uh->end - uh->start;
		if (!bufinsertstr(uh->text, uh->text + tsz, st + uh->start))
			return -1;
		if (storeins(&r, &rh, &ra, rs, st + uh->start, st + uh->end) < 0)
			return -1;
		break;
	}
	free(uh->text);
	uh->text = NULL;
	uh--;
	return 0;
}

static int redosingle()
{
	char *st;
	unsigned tsz;
	st = getbufstart();
	switch (rh->a) {
	case INSERT:
		if (storedel(&u, &uh, &ua, us, st + rh->start, st + rh->end) < 0)
			return -1;
		bufdelete(st + rh->start, st + rh->end);
		break;
	case DELETE:
		tsz = rh->end - rh->start;
		if (!bufinsertstr(rh->text, rh->text + tsz, st + rh->start))
			return -1;
		if (storeins(&u, &uh, &ua, us, st + rh->start, st + rh->end) < 0)
			return -1;
		break;
	}
	free(rh->text);
	rh->text = NULL;
	rh--;
	return 0;
}

static void resetr()
{
	while (rh && rh >= r) {
		free(rh->text);
		rh->text = NULL;
		rh--;
	}
	rs = 0;
}

static int storeins(struct step **l, struct step **h, unsigned *a,
                    unsigned s, char *start, char *end)
{
	assert(inbuf(start) && inbuf(end));
	if (checkalloc(l, h, a) < 0)
		return -1;
	if (!*h)
		*h = *l;
	else
		(*h)++;
	(*h)->a = INSERT;
	(*h)->s = s;
	(*h)->start = start - getbufstart();
	(*h)->end = end - getbufstart();
	(*h)->text = NULL;
	return 0;
}

static int storedel(struct step **l, struct step **h, unsigned *a,
                    unsigned s, char *start, char *end)
{
	assert(inbuf(start) && inbuf(end));
	if (checkalloc(l, h, a) < 0)
		return -1;
	if (!*h)
		*h = *l;
	else
		(*h)++;
	(*h)->a = DELETE;
	(*h)->s = s;
	(*h)->start = start - getbufstart();
	(*h)->end = end - getbufstart();
	(*h)->text = malloc(end - start);
	if (!(*h)->text) {
		(*h)--;
		return -1;
	}
	strncpy((*h)->text, start, end - start);
	return 0;
}

int recinsert(char *start, char *end)
{
	if (storeins(&u, &uh, &ua, us, start, end) < 0)
		return -1;
	resetr();
	return 0;
}

int recdelete(char *start, char *end)
{
	if (storedel(&u, &uh, &ua, us, start, end) < 0)
		return -1;
	resetr();
	return 0;
}

void recstep()
{
	/* Checks the step of the head; don't record empty steps. */
	if (uh && uh->s == us)
		us++;
}

int undo()
{
	if (us == 0)
		return 0;
	us--;
	while (uh && uh >= u && uh->s >= us)
		if (undosingle() < 0)
			return -1;
	rs++;
	return 0;
}

int redo()
{
	if (rs == 0)
		return 0;
	rs--;
	while (rh && rh >= r && rh->s >= rs)
		if (redosingle() < 0)
			return -1;
	us++;
	return 0;
}
