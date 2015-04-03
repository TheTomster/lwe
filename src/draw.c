/* (C) 2015 Tom Wright. */

#include <assert.h>
#include <curses.h>
#include <stdbool.h>

#include "draw.h"
#include "buffer.h"

int lwe_scroll;

struct {
	char *start;
	char *end;
} bounds;
enum {
	UNSET,
	CLEAN,
	DIRTY
} dirty;

int scroll_line()
{
	return lwe_scroll;
}

void set_scroll(int n)
{
	dirty = DIRTY;
	lwe_scroll = n;
	if (lwe_scroll < 0)
		lwe_scroll = 0;
}

static void refresh_bounds(void);

char *winstart()
{
	refresh_bounds();
	return bounds.start;
}

char *winend()
{
	refresh_bounds();
	return bounds.end;
}

static void refresh_bounds()
{
	if (dirty == CLEAN) return;
	bounds.start = skipscreenlines(getbufptr(), scroll_line());
	int r = 0, c = 0;
	bounds.end = bounds.start;
	while (bounds.end != getbufend() && r < LINES - 1) {
		c++;
		if (*bounds.end == '\n')
			c = 0;
		c %= COLS;
		if (c == 0)
			r++;
		bounds.end++;
	}
	if (bounds.end > bounds.start && *bounds.end == '\n') bounds.end--;
	assert(inbuf(bounds.start) && inbuf(bounds.end));
	dirty = CLEAN;
}

char *skipscreenlines(char *start, int lines)
{
	assert(inbuf(start));
	while (lines > 0 && start < getbufend()) {
		lines -= screenlines(start);
		start = endofline(start) + 1;
	}
	start = start > getbufend() ? getbufend() : start;
	return start;
}

int screenlines(char *start)
{
	char *end = endofline(start);
	if (start == NULL || end == NULL)
		return 1;
	int len = end - start;
	return (len / COLS) + 1;
}
