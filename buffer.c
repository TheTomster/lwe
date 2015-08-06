/* buffer.c (c) 2015 Tom Wright */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "buffer.h"
#include "err.h"

static char *buffer;
static int allocatedsz, contentsz;

static int initbuf(int sz);
static int filetobuf(char *path, int sz);
static int overalloc_sz(void);
static int bufextend(void);

static int initbuf(int sz)
{
	allocatedsz = sz + 4096;
	contentsz = 0;
	buffer = malloc(allocatedsz);
	if (buffer == NULL) {
		seterr("memory");
		return -1;
	}
	return 0;
}

static int filetobuf(char *path, int sz)
{
	FILE *f = fopen(path, "r");
	if (f == NULL) {
		seterr("read");
		return -1;
	}
	int readsz = fread(buffer, 1, sz, f);
	if (readsz != sz) {
		seterr("read");
		fclose(f);
		return -1;
	}
	contentsz += sz;
	fclose(f);
	return 0;
}

static int overalloc_sz(void)
{
	return allocatedsz - contentsz;
}

static int bufextend(void)
{
	int newsize = allocatedsz * 2;
	buffer = realloc(buffer, newsize);
	if (buffer == NULL) {
		seterr("memory");
		return -1;
	}
	allocatedsz = newsize;
	return 0;
}

int bufread(char *path)
{
	if (buffer != NULL)
		free(buffer);
	struct stat st;
	errno = 0;
	stat(path, &st);
	if (errno == 0) {
		if (initbuf(st.st_size) < 0)
			return -1;
		return filetobuf(path, st.st_size);
	} else if (errno == ENOENT) {
		return initbuf(0);
	} else {
		seterr(strerror(errno));
		return -1;
	}
}

int bufwrite(char *path)
{
	FILE *f = fopen(path, "w");
	if (f == NULL)
		return -1;
	fwrite(buffer, 1, contentsz, f);
	fclose(f);
	return 0;
}

int bufinsert(char c, char *t)
{
	assert(overalloc_sz() > 0);
	int sztomove = contentsz - (t - buffer);
	memmove(t + 1, t, sztomove);
	*t = c;
	contentsz++;
	if (overalloc_sz() == 0)
		return bufextend();
	else
		return 0;
}

int bufinsertstr(char *start, char *end, char *t)
{
	assert(inbuf(t));
	assert(end >= start);
	char *i;
	unsigned o;
	o = t - getbufstart();
	if (overalloc_sz() < (end - start)) {
		if (!bufextend())
			return -1;
	}
	t = getbufstart() + o;
	for (i = start; i < end; i++)
		if (bufinsert(*i, t++) < 0)
			return -1;
	return 0;
}

void bufdelete(char *start, char *end)
{
	assert(end >= start);
	assert(inbuf(start) && inbuf(end));
	int sztomove = buffer + contentsz - end;
	int szdeleted = end - start;
	memmove(start, end, sztomove);
	contentsz -= szdeleted;
}

char *getbufstart(void)
{
	return buffer;
}

char *getbufend(void)
{
	return buffer + contentsz;
}

char *endofline(char *p)
{
	assert(inbuf(p));
	char *end = memchr(p, '\n', getbufend() - p);
	assert(end == NULL || inbuf(end));
	return end == NULL ? getbufend() : end;
}
