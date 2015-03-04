/* A unique cursorless text editor. (c) 2015 Tom Wright */
#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "err.h"

#define C_D 4
#define C_U 21
#define C_W 23

static char *filename, *start, *end;
static int lwe_scroll;
static char *yanks[26];
static int yanksizes[26];

#define inbuf(p) (p >= getbufptr() && p <= getbufend())
#define bufempty() (getbufptr() == getbufend())

static char *endofline(char *p)
{
	assert(inbuf(p));
	for (; p != getbufend() && *p != '\n'; p++);
	return p;
}

static int screenlines(char *start)
{
	char *end = endofline(start);
	if (start == NULL || end == NULL)
		return 1;
	int len = end - start;
	return (len / COLS) + 1;
}

static char *skipscreenlines(char *start, int lines)
{
	assert(inbuf(start));
	while (lines > 0 && start < getbufend()) {
		lines -= screenlines(start);
		start = endofline(start) + 1;
	}
	start = start > getbufend() ? getbufend() : start;
	return start;
}

static void winbounds(void)
{
	int r = 0, c = 0;
	start = skipscreenlines(getbufptr(), lwe_scroll);
	for (end = start; end != getbufend() && r < LINES; end++) {
		c++;
		if (*end == '\n')
			c = 0;
		c %= COLS;
		if (c == 0)
			r++;
	}
	if (end > start) end--;
	assert(inbuf(start) && inbuf(end));
}

static void pc(char c)
{
	if (!isgraph(c) && !isspace(c))
		c = '?';
	addch(c);
}

static void draw(void)
{
	char *i;
	erase();
	move(0, 0);
	winbounds();
	for (i = start; i < end; i++)
		pc(*i);
	refresh();
}

static void doscrl(int d)
{
	lwe_scroll += d;
	if (lwe_scroll < 0)
		lwe_scroll = 0;
}

static void jumptoline(void)
{
	char buf[32], c;
	int i;
	memset(buf, '\0', 32);
	for (i = 0; i < 32; i++) {
		c = getch();
		if (isdigit(c))
			buf[i] = c;
		else
			break;
	}
	i = atoi(buf);
	if (i == 0)
		return;
	lwe_scroll = i;
	doscrl(-LINES / 2);
}

static char *find(char c, int n)
{
	char *i;
	for (i = start; i < end; i++) {
		if (*i == c) {
			if (n <= 0)
				return i;
			else
				n--;
		}
	}
	return 0;
}

static int count(char c)
{
	int ct;
	char *i;
	ct = 0;
	for (i = start; i != end; i++)
		if (*i == c)
			ct++;
	return ct;
}

static void ptarg(int count)
{
	char a;
	a = 'a' + (count % 26);
	attron(A_STANDOUT);
	pc(a);
	attroff(A_STANDOUT);
}

static int skips(int lvl)
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

static void drawdisambchar(char c, int toskip, char *i, int *tcount)
{
	if (*i == c && toskip <= 0) {
		ptarg(*tcount);
		(*tcount)++;
	} else {
		pc(*i);
	}
}

static void drawdisamb(char c, int lvl, int toskip)
{
	erase();
	move(0, 0);
	int tcount = 0;
	for (char *i = start; i < end; i++) {
		drawdisambchar(c, toskip, i, &tcount);
		if (*i != c)
			continue;
		toskip = (toskip > 0) ? (toskip - 1) : skips(lvl);
	}
	refresh();
}

static bool onlymatch(char c, int lvl, int toskip)
{
	// If the initial skip + this level's skip in between matches is
	// greater than the count of matching characters in the window,
	// then we have narrowed it down to just one choice.  The second
	// match would have to be past the end of the window.
	return skips(lvl) + toskip + 1 >= count(c);
}

static char *disamb(char c)
{
	int lvl = 0;
	int toskip = 0;
	while (!onlymatch(c, lvl, toskip)) {
		drawdisamb(c, lvl, toskip);
		char input = getch();
		int i = input - 'a';
		if (i < 0 || i >= 26)
			return NULL;
		toskip += i * (skips(lvl) + 1);
		lvl++;
	}
	return find(c, toskip);
}

static char *hunt(void)
{
	char c;
	if (bufempty())
		return getbufptr();
	draw();
	c = getch();
	return disamb(c);
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

static void drawlinelbls(int lvl, int off)
{
	draw();
	int count = 0;
	char *p = start;
	int toskip = off;
	for (int line = 0; line < LINES;) {
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

static bool lineselected(int lvl, int off)
{
	return off + skips(lvl) > LINES;
}

static int getoffset(int lvl, int off)
{
	char c = getch();
	int i = c - 'a';
	if (i < 0 || i >= 26)
		return -1;
	int delta = (skips(lvl) + 1) * i;
	return off + delta;
}

static int linehunt(void)
{
	int lvl = 0;
	int off = 0;
	if (bufempty())
		return -1;
	while (!lineselected(lvl, off)) {
		drawlinelbls(lvl, off);
		off = getoffset(lvl, off);
		if (off < 0)
			return -1;
		lvl++;
	}
	return off;
}

static void shiftring(void)
{
	if (yanks[25] != NULL)
		free(yanks[25]);
	memmove(&yanks[1], &yanks[0], sizeof(yanks[0]) * 25);
	memmove(&yanksizes[1], &yanksizes[0], sizeof(yanksizes[0]) * 25);
}

static void yank(char *start, char *end)
{
	shiftring();
	int sz = end - start;
	assert(sz >= 0);
	yanksizes[0] = sz;
	yanks[0] = malloc(sz);
	memcpy(yanks[0], start, sz);
}

static void delete(char *start, char *end)
{
	if (end != getbufend())
		end++;
	yank(start, end);
	bufdelete(start, end);
}

static void ruboutword(char **t)
{
	// Don't delete letter that our cursor is on otherwise we would
	// remove the letter after our last entered character in insert mode
	char *dend = *t;
	char *dstart = dend;
	while (isspace(*dstart) && (dstart > getbufptr()))
		dstart--;
	while (!isspace(*dstart) && (dstart > getbufptr()))
		dstart--;
	// Preserve space before cursor when we can, looks better
	if (dstart != getbufptr() && ((dstart + 1) < dend))
		dstart++;
	delete(dstart, dend);
	*t = dstart;
}

static int insertmode(char *t)
{
	int c;
	for (;;) {
		draw();
		if (t > end)
			doscrl(LINES / 2);
		c = getch();
		if (c == '\r')
			c = '\n';
		if (c == C_D)
			return 1;
		if (c == KEY_BACKSPACE) {
			if (t <= getbufptr())
				continue;
			t--;
			bufdelete(t, t + 1);
			continue;
		}
		if (c == C_W) {
			if (t <= getbufptr())
				continue;
			t--;
			ruboutword(&t);
			continue;
		}
		if (!isgraph(c) && !isspace(c)) {
			continue;
		}
		if (!bufinsert(c, t))
			return 0;
		else
			t++;
	}
	return 1;
}

enum loopsig {
	LOOP_SIGCNT,
	LOOP_SIGQUIT,
	LOOP_SIGERR
};

static enum loopsig checksig(bool ok)
{
	return ok ? LOOP_SIGCNT : LOOP_SIGERR;
}

typedef enum loopsig (*command_fn) (void);

static enum loopsig scrolldown(void)
{
	doscrl(LINES / 2);
	return LOOP_SIGCNT;
}

static enum loopsig scrollup(void)
{
	doscrl(-LINES / 2);
	return LOOP_SIGCNT;
}

static enum loopsig quitcmd(void)
{
	return LOOP_SIGQUIT;
}

static enum loopsig insertcmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return false;
	return checksig(insertmode(start));
}

static enum loopsig appendcmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return false;
	if (start != getbufend())
		start++;
	return checksig(insertmode(start));
}

static enum loopsig writecmd(void)
{
	if (!bufwrite(filename)) {
		const char msg1[] = "Error: Failed to write to: ";
		const char msg2[] = "Press any key to continue.";
		char nstr[strlen(msg1) + strlen(filename) + 1];
		snprintf(nstr, sizeof(nstr), "%s%s", msg1, filename);
		attron(A_STANDOUT);
		mvaddstr(LINES-2, 0, nstr);
		mvaddstr(LINES-1, 0, msg2);
		attroff(A_STANDOUT);
		refresh();
		getch();
	}
	return LOOP_SIGCNT;
}

static void orient(char **start, char **end)
{
	if (*end < *start) {
		char *tmp = *end;
		*end = *start;
		*start = tmp;
	}
}

static enum loopsig deletecmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return LOOP_SIGCNT;
	char *end = hunt();
	if (end == NULL)
		return LOOP_SIGCNT;
	orient(&start, &end);
	delete(start, end);
	return LOOP_SIGCNT;
}

static enum loopsig changecmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return LOOP_SIGCNT;
	char *end = hunt();
	if (end == NULL)
		return LOOP_SIGCNT;
	orient(&start, &end);
	delete(start, end);
	return checksig(insertmode(start));
}

static enum loopsig reloadcmd(void)
{
	bool ok = bufread(filename);
	if (!ok)
		return LOOP_SIGERR;
	return LOOP_SIGCNT;
}

static enum loopsig jumptolinecmd(void)
{
	jumptoline();
	return LOOP_SIGCNT;
}

static void orienti(int *a, int *b)
{
	if (*b < *a) {
		int tmp = *a;
		*a = *b;
		*b = tmp;
	}
}

struct linerange {
	char *start;
	char *end;
};

static char *startofline(int off)
{
	char *result = start;
	while (off > 0) {
		if (!inbuf(result))
			return NULL;
		if (*result == '\n')
			off--;
		result++;
	}
	return result;
}

static struct linerange huntlinerange(void)
{
	int startoffset = linehunt();
	if (startoffset == -1)
		goto retnull;
	int endoffset = linehunt();
	if (endoffset == -1)
		goto retnull;
	orienti(&startoffset, &endoffset);
	char *start = startofline(startoffset);
	if (start == NULL)
		goto retnull;
	char *lstart = startofline(endoffset);
	if (lstart == NULL)
		goto retnull;
	char *end = endofline(lstart);
	return (struct linerange) {.start = start, .end = end};
retnull:
	return (struct linerange) {.start = NULL, .end = NULL};
}

static enum loopsig deletelinescmd(void)
{
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

static enum loopsig changelinescmd(void)
{
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	delete(r.start, r.end);
	return checksig(insertmode(r.start));
}

static enum loopsig lineoverlaycmd(void)
{
	winbounds();
	int lineno = lwe_scroll + 1;
	int screenline = 0;
	int fileline = 0;
	attron(A_STANDOUT);
	while (screenline < LINES) {
		char nstr[32];
		snprintf(nstr, sizeof(nstr), "%4d", lineno);
		mvaddstr(screenline, 0, nstr);
		char *lstart = startofline(fileline);
		if (lstart == NULL)
			break;
		screenline += screenlines(lstart);
		fileline++;
		lineno++;
	}
	attroff(A_STANDOUT);
	refresh();
	getch();
	return LOOP_SIGCNT;
}

static enum loopsig yankcmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return LOOP_SIGCNT;
	char *end = hunt();
	if (end == NULL)
		return LOOP_SIGCNT;
	yank(start, end);
	return LOOP_SIGCNT;
}

static enum loopsig yanklinescmd(void)
{
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank(r.start, r.end);
	return LOOP_SIGCNT;
}

struct yankstr {
	char *start;
	char *end;
};

static struct yankstr yankhunt(void)
{
	clear();
	int linestodraw = 26 < LINES ? 26 : LINES;
	for (int i = 0; i < linestodraw; i++) {
		attron(A_STANDOUT);
		mvaddch(i, 0, 'a' + i);
		attroff(A_STANDOUT);
		int previewsz = COLS - 2;
		char preview[previewsz];
		snprintf(preview, previewsz, "%s", yanks[i]);
		for (int j = 0; j < yanksizes[i] && j < previewsz; j++) {
			char c = preview[j];
			c = (isgraph(c) || c == ' ') ? c : '?';
			addch(c);
		}
	}
	refresh();
	int selected = getch() - 'a';
	if (selected < 0 || selected > 25)
		return (struct yankstr) {NULL, NULL};
	struct yankstr result;
	result.start = yanks[selected];
	result.end = result.start + yanksizes[selected];
	return result;
}

static enum loopsig preputcmd(void)
{
	char *t = hunt();
	if (t == NULL)
		return LOOP_SIGCNT;
	struct yankstr y = yankhunt();
	if (y.start == NULL || y.end == NULL)
		return LOOP_SIGCNT;
	return checksig(bufinsertstr(y.start, y.end, t));
}

static enum loopsig putcmd(void)
{
	char *t = hunt();
	if (t == NULL)
		return LOOP_SIGCNT;
	if (t != getbufend())
		t++;
	struct yankstr y = yankhunt();
	if (y.start == NULL || y.end == NULL)
		return LOOP_SIGCNT;
	return checksig(bufinsertstr(y.start, y.end, t));
}

static command_fn cmdtbl[512] = {
	[C_D] = scrolldown,
	[KEY_DOWN] = scrolldown,
	[KEY_NPAGE] = scrolldown,
	['j'] = scrolldown,
	[C_U] = scrollup,
	[KEY_UP] = scrollup,
	[KEY_PPAGE] = scrollup,
	['k'] = scrollup,
	['q'] = quitcmd,
	['i'] = insertcmd,
	['a'] = appendcmd,
	['w'] = writecmd,
	['d'] = deletecmd,
	['c'] = changecmd,
	['r'] = reloadcmd,
	['g'] = jumptolinecmd,
	['D'] = deletelinescmd,
	['C'] = changelinescmd,
	['n'] = lineoverlaycmd,
	['y'] = yankcmd,
	['Y'] = yanklinescmd,
	['p'] = putcmd,
	['o'] = preputcmd
};

static int cmdloop(void)
{
	for (;;) {
		draw();
		int c = getch();
		command_fn cmd = cmdtbl[c];
		if (cmd == NULL)
			continue;
		enum loopsig s = cmd();
		if (s == LOOP_SIGQUIT)
			return 1;
		else if (s == LOOP_SIGERR)
			return 0;
	}
	return 0;
}

static void ed(void)
{
	if (!bufread(filename))
		return;
	lwe_scroll = 0;
	cmdloop();
}

int main(int argc, char **argv)
{
	char errbuf[256];
	if (argc != 2) {
		seterr("missing file arg");
		goto error;
	} else {
		initscr();
		cbreak();
		noecho();
		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		filename = argv[1];
		ed();

		endwin();
	}
	error:
	geterr(errbuf, sizeof(errbuf));
	if (errbuf[0] == '\0')
		return 0;
	fprintf(stderr, "error: %s\n", errbuf);
	return 1;
}
