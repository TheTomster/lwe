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

#define LLWE_CYAN 1

char *filename, *start, *end, *mode;
int lwe_scroll;
char *yanks[26];
int yanksizes[26];

#define inbuf(p) (p >= getbufptr() && p <= getbufend())
#define bufempty() (getbufptr() == getbufend())

char *startofline(int off)
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

char *endofline(char *p)
{
	assert(inbuf(p));
	for (; p != getbufend() && *p != '\n'; p++);
	return p;
}

int screenlines(char *start)
{
	char *end = endofline(start);
	if (start == NULL || end == NULL)
		return 1;
	int len = end - start;
	return (len / COLS) + 1;
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

void winbounds(void)
{
	int r = 0, c = 0;
	start = skipscreenlines(getbufptr(), lwe_scroll);
	for (end = start; end != getbufend() && r < LINES - 1; end++) {
		c++;
		if (*end == '\n')
			c = 0;
		c %= COLS;
		if (c == 0)
			r++;
	}
	if (end > start && *end == '\n') end--;
	assert(inbuf(start) && inbuf(end));
}

void pc(char c)
{
	if (c == '\r')
		c = '?';
	if (!isgraph(c) && !isspace(c))
		c = '?';
	addch(c);
}

void drawmodeline(void)
{
	int r = LINES - 1;
	char buf[8192];
	snprintf(buf, sizeof(buf), "[F: %-32.32s][M: %-24s][L: %8d]", filename, mode, lwe_scroll);
	attron(COLOR_PAIR(LLWE_CYAN));
	mvaddstr(r, 0, buf);
	attroff(COLOR_PAIR(LLWE_CYAN));
}

void draw(void)
{
	char *i;
	erase();
	move(0, 0);
	winbounds();
	for (i = start; i < end; i++)
		pc(*i);
	drawmodeline();
	refresh();
}

void doscrl(int d)
{
	lwe_scroll += d;
	if (lwe_scroll < 0)
		lwe_scroll = 0;
}

char *find(char c, int n)
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

int count(char c)
{
	int ct;
	char *i;
	ct = 0;
	for (i = start; i != end; i++)
		if (*i == c)
			ct++;
	return ct;
}

int findcharcount(char *s, char *e, char c)
{
	int count = 0;
	if(!s || !e){
		return count;
	}
	while(s != e){
		if(*s == c){
			count++;
		}
		s++;
	}
	return count;
}

void ptarg(int count)
{
	char a;
	a = 'a' + (count % 26);
	attron(A_STANDOUT);
	pc(a);
	attroff(A_STANDOUT);
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

void drawdisambchar(char c, int toskip, char *i, int *tcount)
{
	if (*i == c && toskip <= 0) {
		ptarg(*tcount);
		(*tcount)++;
	} else {
		pc(*i);
	}
}

void drawdisamb(char c, int lvl, int toskip)
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
	drawmodeline();
	refresh();
}

bool onlymatch(char c, int lvl, int toskip)
{
	// If the initial skip + this level's skip in between matches is
	// greater than the count of matching characters in the window,
	// then we have narrowed it down to just one choice.  The second
	// match would have to be past the end of the window.
	return skips(lvl) + toskip + 1 >= count(c);
}

char *disamb(char c)
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

char *hunt(void)
{
	char c;
	if (bufempty())
		return getbufptr();
	draw();
	c = getch();
	return disamb(c);
}

void nextline(char **p)
{
	assert(inbuf(*p));
	for (;*p < getbufend(); (*p)++)
		if (**p == '\n') {
			(*p)++;
			break;
		}
	assert(inbuf(*p));
}

void drawlinelbls(int lvl, int off)
{
	draw();
	int count = 0;
	char *p = start;
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

bool lineselected(int lvl, int off)
{
	return off + skips(lvl) > LINES;
}

int getoffset(int lvl, int off)
{
	char c = getch();
	int i = c - 'a';
	if (i < 0 || i >= 26)
		return -1;
	int delta = (skips(lvl) + 1) * i;
	return off + delta;
}

int linehunt(void)
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

void shiftring(void)
{
	if (yanks[25] != NULL)
		free(yanks[25]);
	memmove(&yanks[1], &yanks[0], sizeof(yanks[0]) * 25);
	memmove(&yanksizes[1], &yanksizes[0], sizeof(yanksizes[0]) * 25);
}

void yank(char *start, char *end)
{
	shiftring();
	int sz = end - start;
	assert(sz >= 0);
	yanksizes[0] = sz;
	yanks[0] = malloc(sz);
	memcpy(yanks[0], start, sz);
}

void delete(char *start, char *end)
{
	if (end != getbufend())
		end++;
	yank(start, end);
	bufdelete(start, end);
}

void ruboutword(char **t)
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

void movecursor(char *t){
	if (!t) 
		return;
	// Figure out line number, number of tabs on the line,
	// along with how much space each tab is actually taking up
	// so we can figure out the offset in order to highlight the cursor
	int offset = 0;
	int linenumber = findcharcount(start, t, '\n');
	char* linestart = startofline(linenumber);
	if (linestart) {
		char* iter = linestart;
			while (iter != t) {
				if (*iter == '\t')
					offset += (TABSIZE - (offset % TABSIZE));
				else
					offset++;
				iter++;
			}
		move(linenumber, offset);
	}
}

int insertmode(char *t)
{
	mode = "INSERT";
	int c;
	for (;;) {
		draw();
		if (t > end)
			doscrl(LINES / 2);
		movecursor(t);
		c = getch();
		if (c == '\r')
			c = '\n';
		if (c == C_D)
			return 1;
		if (c == KEY_BACKSPACE || c == 127) {
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

enum loopsig checksig(bool ok)
{
	return ok ? LOOP_SIGCNT : LOOP_SIGERR;
}

typedef enum loopsig (*command_fn) (void);

enum loopsig scrolldown(void)
{
	doscrl(LINES / 2);
	return LOOP_SIGCNT;
}

enum loopsig scrollup(void)
{
	doscrl(-LINES / 2);
	return LOOP_SIGCNT;
}

enum loopsig quitcmd(void)
{
	return LOOP_SIGQUIT;
}

enum loopsig insertcmd(void)
{
	mode = "TARGET (INSERT)";
	char *start = hunt();
	if (start == NULL)
		return false;
	return checksig(insertmode(start));
}

enum loopsig appendcmd(void)
{
	mode = "TARGET (APPEND)";
	char *start = hunt();
	if (start == NULL)
		return false;
	if (start != getbufend())
		start++;
	return checksig(insertmode(start));
}

enum loopsig writecmd(void)
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

void orient(char **start, char **end)
{
	if (*end < *start) {
		char *tmp = *end;
		*end = *start;
		*start = tmp;
	}
}

enum loopsig deletecmd(void)
{
	mode = "TARGET (DELETE)";
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

enum loopsig changecmd(void)
{
	mode = "TARGET (CHANGE)";
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

enum loopsig reloadcmd(void)
{
	bool ok = bufread(filename);
	if (!ok)
		return LOOP_SIGERR;
	return LOOP_SIGCNT;
}

enum loopsig jumptolinecmd(void)
{
	mode = "JUMP";
	draw();
	char buf[32];
	memset(buf, '\0', 32);
	for (int i = 0; i < 32; i++) {
		char c = getch();
		if (isdigit(c))
			buf[i] = c;
		else
			break;
	}
	int i = atoi(buf);
	if (i == 0)
		return LOOP_SIGCNT;
	lwe_scroll = i;
	doscrl(-LINES / 2);
	return LOOP_SIGCNT;
}

void orienti(int *a, int *b)
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

struct linerange huntlinerange(void)
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

enum loopsig deletelinescmd(void)
{
	mode = "TARGET LINES (DELETE)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

enum loopsig changelinescmd(void)
{
	mode = "TARGET LINES (CHANGE)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	delete(r.start, r.end);
	return checksig(insertmode(r.start));
}

enum loopsig lineoverlaycmd(void)
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

enum loopsig yankcmd(void)
{
	mode = "TARGET (YANK)";
	char *start = hunt();
	if (start == NULL)
		return LOOP_SIGCNT;
	char *end = hunt();
	if (end == NULL)
		return LOOP_SIGCNT;
	yank(start, end);
	return LOOP_SIGCNT;
}

enum loopsig yanklinescmd(void)
{
	mode = "TARGET LINES (YANK)";
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

struct yankstr yankhunt(void)
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

enum loopsig preputcmd(void)
{
	mode = "TARGET (PRE-PUT)";
	char *t = hunt();
	if (t == NULL)
		return LOOP_SIGCNT;
	struct yankstr y = yankhunt();
	if (y.start == NULL || y.end == NULL)
		return LOOP_SIGCNT;
	return checksig(bufinsertstr(y.start, y.end, t));
}

enum loopsig putcmd(void)
{
	mode = "TARGET (PUT)";
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

command_fn cmdtbl[512] = {
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

int cmdloop(void)
{
	for (;;) {
		mode = "COMMAND";
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

int main(int argc, char **argv)
{
	char errbuf[256];
	if (argc != 2) {
		seterr("missing file arg");
		goto error;
	} else {
		initcurses();
		filename = argv[1];
		if (bufread(filename)) {
			lwe_scroll = 0;
			cmdloop();
		}
		endwin();
	}
	error:
	geterr(errbuf, sizeof(errbuf));
	if (errbuf[0] == '\0')
		return 0;
	fprintf(stderr, "error: %s\n", errbuf);
	return 1;
}
