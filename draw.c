/* (C) 2015 Tom Wright. */

#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <string.h>

#include "draw.h"
#include "buffer.h"
#include "yank.h"

#define MODELINE 1
#define WHITESPACE 2
#define TARGET 3

int show_whitespace;

static char *scroll_ptr;
static int scroll_linum;

struct {
	char *start;
	char *end;
} bounds;

static void pc(char c);
static void ptarg(int count);
static char *nextline(char *p);
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
	return scroll_linum;
}

void set_scroll(int n)
{
	scroll_linum = n;
	if (scroll_linum < 0)
		scroll_linum = 0;
	scroll_ptr = getbufstart();
	for (int i = 0; scroll_ptr != getbufend() && i < scroll_linum; i++)
		scroll_ptr = nextline(scroll_ptr);
	refresh_bounds();
}

void adjust_scroll(int delta)
{
	set_scroll(scroll_line() + delta);
}

char *winstart()
{
	return bounds.start;
}

char *winend()
{
	return bounds.end;
}

int winrows()
{
	return LINES;
}

int wincols()
{
	return COLS;
}

void refresh_bounds()
{
	bounds.start = scroll_ptr;
	int r = 0;
	bounds.end = bounds.start;
	while (bounds.end != getbufend() && r < LINES - 1) {
		r += screenlines(bounds.end);
		bounds.end = nextline(bounds.end);
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
	int len = 0;
	for (char *i = start; i != end; i++) {
		if (*i == '\t') {
			int to_tab = TABSIZE - (len % TABSIZE);
			len += to_tab;
		} else {
			len++;
		}
	}
	return (len / COLS) + 1;
}

static void pc(char c)
{
	if (c == '\r')
		c = '?';
	else if (!isgraph(c) && !isspace(c))
		c = '?';
	if (show_whitespace && c == ' ') {
		attron(COLOR_PAIR(WHITESPACE));
		addch('.');
		attroff(COLOR_PAIR(WHITESPACE));
	} else if (show_whitespace && c == '\n') {
		attron(COLOR_PAIR(WHITESPACE));
		addch('$');
		addch('\n');
		attroff(COLOR_PAIR(WHITESPACE));
	} else if (show_whitespace && c == '\t') {
		attron(COLOR_PAIR(WHITESPACE));
		for (int i = 0; i < TABSIZE; i++)
			addch('-');
		attroff(COLOR_PAIR(WHITESPACE));
	} else {
		addch(c);
	}
}

void drawmodeline(char *filename, char *mode)
{
	int r = LINES - 1;
	char buf[8192];
	attron(COLOR_PAIR(1));
	snprintf(buf, sizeof(buf), "[F: %-32.32s][M: %-24s][L: %8d]",
			filename, mode, scroll_line());
	mvaddstr(r, 0, buf);
	attroff(COLOR_PAIR(MODELINE));
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
#if NCURSES_REENTRANT
	set_escdelay(25);
#else
	ESCDELAY = 25;
#endif
	start_color();
	init_pair(MODELINE, COLOR_CYAN, COLOR_BLACK);
	init_pair(WHITESPACE, COLOR_BLUE, COLOR_BLACK);
	init_pair(TARGET, COLOR_BLACK, COLOR_GREEN);
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
	attron(COLOR_PAIR(TARGET));
	pc(a);
	attroff(COLOR_PAIR(TARGET));
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
		p = nextline(p);
	}
}

static char *nextline(char *p)
{
	assert(inbuf(p));
        char *nl = memchr(p, '\n', getbufend() - p);
	if (nl == NULL)
		return getbufend();
	if (nl != getbufend())
		nl++;
	assert(inbuf(nl));
	return nl;
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

int onlymatch(char c, int lvl, int toskip)
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
	attron(COLOR_PAIR(TARGET));
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
	attroff(COLOR_PAIR(TARGET));
}

void drawmessage(char *msg)
{
	assert(msg != NULL);
	attron(COLOR_PAIR(MODELINE));
	mvaddstr(LINES - 1, 0, msg);
	attroff(COLOR_PAIR(MODELINE));
}

void drawyanks()
{
	char *ytext;
	unsigned lines, nyanks, linestodraw, ysz, previewsz, i, j;
	char c;
	nyanks = yank_sz();
	lines = LINES;
	linestodraw = nyanks < lines ? nyanks : lines;
	for (i = 0; i < linestodraw; i++) {
		attron(COLOR_PAIR(TARGET));
		mvaddch(i, 0, 'a' + i);
		attroff(COLOR_PAIR(TARGET));
		yank_item(&ytext, &ysz, i);
		previewsz = COLS - 2;
		if (ysz < previewsz)
			previewsz = ysz;
		for (j = 0; j < previewsz; j++) {
			c = ytext[j];
			c = (isgraph(c) || c == ' ') ? c : '?';
			addch(c);
		}
	}
}

void draw_eof(void)
{
	char *start = skipscreenlines(getbufstart(), scroll_line());
	int r = 0;
	while(start != getbufend() && r < LINES - 1) {
		r += screenlines(start);
		start = skipscreenlines(start,1);
	}

	for(; r < LINES - 1; ++r)
		mvaddch(r,0,'~');
	move(0,0);
}

void movecursor(char *p)
{
	assert(inbuf(p));
	move(0, 0);
	for (char *i = winstart(); i < winend() && i < p; i++)
		advcursor(*i);
}
