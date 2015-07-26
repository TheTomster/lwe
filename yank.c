/* (C) 2015 Tom Wright */

#include <assert.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "yank.h"

#define N_YANKS 26
#define LAST_YANK (N_YANKS - 1)
#define YANK_FILE "/tmp/lwe_yank_"

static unsigned yank_sizes[N_YANKS];
static char *yank_buffers[N_YANKS];

static void shiftyanks(void);

/*
 * Yanked text is stored in malloc'd buffers.  The yanks array is an
 * array of pointers to these buffers.  When a new item is yanked, we
 * want to shift everything down, dropping the last item in order to
 * make room for the new item.
 */
static void shiftyanks()
{
	if (yank_buffers[LAST_YANK] != NULL)
		free(yank_buffers[LAST_YANK]);
	memmove(&yank_buffers[1], &yank_buffers[0],
			sizeof(yank_buffers[0]) * LAST_YANK);
	memmove(&yank_sizes[1], &yank_sizes[0],
			sizeof(yank_sizes[0]) * LAST_YANK);
}

int saveyanks()
{
	char filename[8192];
	struct passwd *pwd;
	FILE *f;
	size_t w;
	int i, err;
	uid_t u;
	u = getuid();
	if (!(pwd = getpwuid(u)))
		return -1;
	strlcpy(filename, YANK_FILE, sizeof(filename));
	strlcat(filename, pwd->pw_name, sizeof(filename));
	if (!(f = fopen(filename, "w")))
		return -1;
	err = 0;
	for (i = 0; i < N_YANKS; i++) {
		if (yank_buffers[i] == NULL)
			break;
		if (fprintf(f, "%u\n", yank_sizes[i]) < 0) {
			err = -1;
			goto close;
		}
		w = fwrite(yank_buffers[i], sizeof(char), yank_sizes[i], f);
		if (w < yank_sizes[i]) {
			err = -1;
			goto close;
		}
		if (fputc('\n', f) == EOF) {
			err = -1;
			goto close;
		}
	}
	close:
	fclose(f);
	return err;
}

int loadyanks()
{
	return -1;
}

/*
 * Gets the number of items stored in the yank buffers.
 */
int yank_sz()
{
	int i;
	for (i = 0; i < 26; i++)
		if (yank_buffers[i] == NULL)
			break;
	return i;
}

/*
 * Gets the n'th yank item.  The pointer to the string is stored in the
 * location pointed to by item, and the length is stored in the location
 * pointed to by len.  n should be in the interval [0, yank_sz).
 */
void yank_item(char **item, int *len, int n)
{
	assert(n >= 0);
	assert(n < yank_sz());
	*item = yank_buffers[n];
	*len = yank_sizes[n];
}

/*
 * Stores a string in the yank buffers.
 */
void yank_store(char *start, char *end)
{
	shiftyanks();
	int sz = end - start;
	assert(sz >= 0);
	yank_sizes[0] = sz;
	yank_buffers[0] = malloc(sz);
	memcpy(yank_buffers[0], start, sz);
}
