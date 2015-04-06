/* (C) 2015 Tom Wright. */

#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <stdbool.h>

#include "draw.h"
#include "buffer.h"

#define LLWE_CYAN 1

int lwe_scroll;

struct {
	char *start;
	char *end;
} bounds;

int scroll_line()
{
	return lwe_scroll;
}

void set_scroll(int n)
{
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

void pc(char c)
{
	if (c == '\r')
		c = '?';
	if (!isgraph(c) && !isspace(c))
		c = '?';
	addch(c);
}

void drawmodeline(char *filename, char *mode)
{
	int r = LINES - 1;
	char buf[8192];
	snprintf(buf, sizeof(buf), "[F: %-32.32s][M: %-24s][L: %8d]",
			filename, mode, scroll_line());
	attron(COLOR_PAIR(LLWE_CYAN));
	mvaddstr(r, 0, buf);
	attroff(COLOR_PAIR(LLWE_CYAN));
}

void old_draw(char *filename, char *mode)
{
	char *i;
	erase();
	move(0, 0);
	for (i = winstart(); i < winend(); i++)
		pc(*i);
	drawmodeline(filename, mode);
	refresh();
}

void initcurses()
{
	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	start_color();
	init_pair(LLWE_CYAN, COLOR_CYAN, COLOR_BLACK);
}

