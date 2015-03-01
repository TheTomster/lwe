/* buffer.c (c) 2015 Tom Wright */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "err.h"

#define inbuf(p) (p >= buffer && p <= buffer + contentsz)

static char *buffer;
static int allocatedsz, contentsz;

static bool initbuf(int sz);
static bool filetobuf(char *path, int sz);
static int overalloc_sz(void);
static bool bufextend(void);

bool bufread(char *path)
{
	if (buffer != NULL)
		free(buffer);
	struct stat st;
	errno = 0;
	stat(path, &st);
	if (errno == 0) {
		bool ok = initbuf(st.st_size);
		if (!ok)
			return false;
		return filetobuf(path, st.st_size);
	} else if (errno == ENOENT) {
		return initbuf(0);
	} else {
		seterr(strerror(errno));
		return false;
	}
}

static bool initbuf(int sz)
{
	allocatedsz = sz + 4096;
	contentsz = 0;
	buffer = malloc(allocatedsz);
	if (buffer == NULL) {
		seterr("memory");
		return false;
	}
	return true;
}

static bool filetobuf(char *path, int sz)
{
	FILE *f = fopen(path, "r");
	if (f == NULL) {
		seterr("read");
		return false;
	}
	int readsz = fread(buffer, 1, sz, f);
	if (readsz != sz) {
		seterr("read");
		return false;
	}
	return true;
}

bool bufwrite(char *path)
{
	FILE *f = fopen(path, "w");
	if (f == NULL)
		return false;
	fwrite(buffer, 1, contentsz, f);
	fclose(f);
	return true;
}

static bool bufextend(void)
{
	int newsize = allocatedsz * 2;
	buffer = realloc(buffer, newsize);
	if (buffer == NULL) {
		seterr("memory");
		return false;
	}
	allocatedsz = newsize;
	return true;
}

bool bufinsert(char c, char *t)
{
	assert(overalloc_sz() > 0);
	int sztomove = contentsz - (t - buffer);
	memmove(t + 1, t, sztomove);
	*t = c;
	contentsz++;
	if (overalloc_sz() == 0)
		return bufextend();
	else
		return true;
}

static int overalloc_sz(void)
{
	return allocatedsz - contentsz;
}

bool bufinsertstr(char *start, char *end, char *t)
{
	assert(inbuf(t));
	assert(end >= start);
	for (char *i = start; i < end; i++)
		if (!bufinsert(*i, t++))
			return false;
	return true;
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

char *getbufptr(void)
{
	return buffer;
}

char *getbufend(void)
{
	return buffer + contentsz;
}

