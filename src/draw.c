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

static void ptarg(int count);
static void refresh_bounds(void);
static void drawdisambchar(char c, int toskip, char *i, int *tcount);
static void nextline(char **p);

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

void drawdisamb(char c, int lvl, int toskip, char *filename, char *mode)
{
	erase();
	move(0, 0);
	int tcount = 0;
	for (char *i = winstart(); i < winend(); i++) {
		drawdisambchar(c, toskip, i, &tcount);
		if (*i != c)
			continue;
		toskip = (toskip > 0) ? (toskip - 1) : skips(lvl);
	}
	drawmodeline(filename, mode);
	refresh();
}

static void drawdisambchar(char c, int toskip, char *i, int *tcount)
{
	if (*i == c && toskip <= 0) {
		ptarg(*tcount);
		(*tcount)++;
	} else {
		pc(*i);
	}
}

static void ptarg(int count)
{
	char a;
	a = 'a' + (count % 26);
	attron(A_STANDOUT);
	pc(a);
	attroff(A_STANDOUT);
}

void drawlinelbls(int lvl, int off, char *filename, char *mode)
{
	old_draw(filename, mode);
	int count = 0;
	char *p = winstart();
	int toskip = off;
	for (int line = 0; line < LINES - 1;) {
		if (toskip == 0) {
			move(line, 0);
			ptarg(count++);
			toskip = skips(lvl);
		} else {
			toskip--;
		}
		line += screenlines(p);
		nextline(&p);
	}
	refresh();
}

static void nextline(char **p)
{
	assert(inbuf(*p));
	for (;*p < getbufend(); (*p)++)
		if (**p == '\n') {
			(*p)++;
			break;
		}
	assert(inbuf(*p));
}

int skips(int lvl)
{
	if (lvl == 0)
		return 0;
	int i = 1;
	while (lvl > 0) {
		lvl--;
		i *= 26;
	}
	return i - 1;
}

bool onlymatch(char c, int lvl, int toskip)
{
	// If the initial skip + this level's skip in between matches is
	// greater than the count of matching characters in the window,
	// then we have narrowed it down to just one choice.  The second
	// match would have to be past the end of the window.
	return skips(lvl) + toskip + 1 >= count(c);
}

int countwithin(char *start, char *end, char c)
{
	int ct = 0;
	for (char *i = start; i != end; i++)
		if (*i == c)
			ct++;
	return ct;
}

int count(char c)
{
	return countwithin(winstart(), winend(), c);
}
