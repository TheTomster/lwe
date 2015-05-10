/* (C) 2015 Tom Wright. */

#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <stdbool.h>

#include "draw.h"
#include "buffer.h"
#include "yank.h"

int lwe_scroll;

struct {
	char *start;
	char *end;
} bounds;

static void pc(char c);
static void ptarg(int count);
static void refresh_bounds(void);
static void nextline(char **p);
static void advcursor(char c);

void present(void)
{
	refresh();
}

void clrscreen(void)
{
	erase();
}

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

void adjust_scroll(int delta)
{
	set_scroll(scroll_line() + delta);
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
	assert(inbuf(bounds.start) && inbuf(bounds.end));
}

char *skipscreenlines(char *start, int lines)
{
	assert(inbuf(start));
	while (lines > 0 && start < getbufend()) {
		lines -= screenlines(start);
		start = endofline(start) + 1;
	}
	if (start > getbufend())
		start = getbufend();
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

static void pc(char c)
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
	mvaddstr(r, 0, buf);
}

void drawtext()
{
	char *i;
	erase();
	move(0, 0);
	for (i = winstart(); i < winend(); i++)
		pc(*i);
}

void initcurses()
{
	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	ESCDELAY = 200;
}

static void advcursor(char c)
{
	int row, column;
	getyx(stdscr, row, column);
	if (c == '\n') {
		row++;
		column = 0;
	} else if (c == '\t') {
		do column++; while(column % TABSIZE != 0);
	} else {
		column++;
	}
	if (column >= COLS) {
		row++;
		column = 0;
	}
	move(row, column);
}

void drawdisamb(char c, int lvl, int toskip)
{
	move(0, 0);
	int tcount = 0;
	for (char *i = winstart(); i < winend(); i++) {
		if (*i == c && toskip <= 0)
			ptarg(tcount++);
		else
			advcursor(*i);
		if (*i == c)
			toskip = (toskip > 0) ? (toskip - 1) : skips(lvl);
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

void drawlinelbls(int lvl, int off)
{
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

void drawlineoverlay(void)
{
	int lineno = scroll_line() + 1;
	int screenline = 0;
	int fileline = 0;
	attron(A_STANDOUT);
	while (screenline < LINES) {
		char nstr[32];
		snprintf(nstr, sizeof(nstr), "%4d", lineno);
		mvaddstr(screenline, 0, nstr);
		char *lstart = skipscreenlines(winstart(), fileline);
		if (lstart == NULL)
			break;
		screenline += screenlines(lstart);
		fileline++;
		lineno++;
	}
	attroff(A_STANDOUT);
}

void drawmessage(char *msg)
{
	assert(msg != NULL);
	attron(A_STANDOUT);
	mvaddstr(LINES - 1, 0, msg);
	attroff(A_STANDOUT);
}

void drawyanks()
{
	int nyanks = yank_sz();
	int linestodraw = nyanks < LINES ? nyanks : LINES;
	for (int i = 0; i < linestodraw; i++) {
		attron(A_STANDOUT);
		mvaddch(i, 0, 'a' + i);
		attroff(A_STANDOUT);
		char *ytext;
		int ysz;
		yank_item(&ytext, &ysz, i);
		int previewsz = COLS - 2;
		char preview[previewsz];
		snprintf(preview, previewsz, "%s", ytext);
		for (int j = 0; j < ysz && j < previewsz; j++) {
			char c = preview[j];
			c = (isgraph(c) || c == ' ') ? c : '?';
			addch(c);
		}
	}
}
