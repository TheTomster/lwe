/* (C) 2015 Tom Wright */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "yank.h"

static int yank_sizes[26];
static char *yank_buffers[26];

static void shiftyanks(void);

/* Gets the number of items stored in the yank buffers. */
int yank_sz(void)
{
	int i;
	for (i = 0; i < 26; i++)
		if (yank_buffers[i] == NULL)
			break;
	return i;
}

/* Gets the n'th yank item.  The pointer to the string is stored in the
 * location pointed to by item, and the length is stored in the location
 * pointed to by len.  n should be in the interval [0, yank_sz).  */
void yank_item(char **item, int *len, int n)
{
	assert(n >= 0);
	assert(n < yank_sz());
	*item = yank_buffers[n];
	*len = yank_sizes[n];
}

/* Yanked text is stored in malloc'd buffers.  The yanks array is an
 * array of pointers to these buffers.  When a new item is yanked, we
 * want to shift everything down, dropping the last item in order to
 * make room for the new item. */
static void shiftyanks(void)
{
	if (yank_buffers[25] != NULL)
		free(yank_buffers[25]);
	memmove(&yank_buffers[1], &yank_buffers[0],
			sizeof(yank_buffers[0]) * 25);
	memmove(&yank_sizes[1], &yank_sizes[0],
			sizeof(yank_sizes[0]) * 25);
}

/* Stores a string in the yank buffers. */
void yank_store(char *start, char *end)
{
	shiftyanks();
	int sz = end - start;
	assert(sz >= 0);
	yank_sizes[0] = sz;
	yank_buffers[0] = malloc(sz);
	memcpy(yank_buffers[0], start, sz);
}
