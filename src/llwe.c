/* A unique cursorless text editor. (c) 2015 Tom Wright */
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define C_D 4
#define C_U 21

static char *filename, *buffer, *start, *end;
char errbuf[256];
static int bufsize, gap, lwe_scroll;

static int
gapsize(void)
{
	return bufsize - gap;
}

static void
err(const char *str)
{
	snprintf(errbuf, sizeof(errbuf), "%s", str);
}

static bool
bext(void)
{
	int newsize = bufsize * 2;
	buffer = realloc(buffer, newsize);
	if (buffer == NULL) {
		err("memory");
		return false;
	}
	bufsize = newsize;
	return true;
}

static bool
bins(char c, char *t)
{
	int sz;
	sz = gap - (t - buffer);
	memmove(t + 1, t, sz);
	*t = c;
	gap++;
	if (gapsize() == 0)
		return bext();
	else
		return true;
}

static bool
initbuf(int sz)
{
	bufsize = sz + 4096;
	gap = sz;
	buffer = malloc(bufsize);
	if (buffer == NULL) {
		err("memory");
		return false;
	}
	return true;
}

static bool
filetobuf(int sz)
{
	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		err("read");
		return false;
	}
	int rsz = fread(buffer, 1, sz, f);
	if (rsz != sz) {
		err("read");
		return false;
	}
	return true;
}

static bool
bread(void)
{
	struct stat st;
	errno = 0;
	stat(filename, &st);
	if (errno == 0) {
		bool ok = initbuf(st.st_size);
		if (!ok)
			return false;
		return filetobuf(st.st_size);
	} else if (errno == ENOENT) {
		return initbuf(0);
	} else {
		err(strerror(errno));
		return false;
	}
}

int
breload()
{
	free(buffer);
	return bread();
}

static int
bsave(void)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		err("write");
		return 0;
	}
	fwrite(buffer, 1, bufsize - gapsize(), f);
	fclose(f);
	return 1;
}

static int
isend(const char *s)
{
	return s >= (buffer + bufsize - gapsize());
}

static void
winbounds(void)
{
	int r, c, i;
	r = c = 0;
	for (i = lwe_scroll, start = buffer; i > 0 && !isend(start);
	     (start)++)
		if (*start == '\n')
			i--;
	end = start;
loop:	if (isend(end))
		return;
	c++;
	if (*end == '\n') {
		c = 0;
	}
	c %= COLS;
	if (c == 0)
		r++;
	end++;
	if (r < LINES)
		goto loop;
	else
		end--;
}

static void
pc(char c)
{
	if (!isgraph(c) && !isspace(c))
		c = '?';
	addch(c);
}

static void
draw(void)
{
	char *i;
	erase();
	move(0, 0);
	winbounds();
	for (i = start; i != end; i++)
		pc(*i);
	refresh();
}

static void
doscrl(int d)
{
	lwe_scroll += d;
	if (lwe_scroll < 0)
		lwe_scroll = 0;
}

static void
jumptoline(void)
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

static char *
find(char c, int n)
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

static int
count(char c)
{
	int ct;
	char *i;
	ct = 0;
	for (i = start; i != end; i++)
		if (*i == c)
			ct++;
	return ct;
}

static void
ptarg(int count)
{
	char a;
	a = 'a' + (count % 26);
	attron(A_STANDOUT);
	pc(a);
	attroff(A_STANDOUT);
}

static int
skips(int lvl)
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

static void
drawdisambchar(char c, int toskip, char *i, int *tcount)
{
	if (*i == c && toskip <= 0) {
		ptarg(*tcount);
		(*tcount)++;
	} else {
		pc(*i);
	}
}

static void
drawdisamb(char c, int lvl, int toskip)
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

static bool
onlymatch(char c, int lvl, int toskip)
{
	// If the initial skip + this level's skip in between matches is
	// greater than the count of matching characters in the window,
	// then we have narrowed it down to just one choice.  The second
	// match would have to be past the end of the window.
	return skips(lvl) + toskip >= count(c);
}

static char *
disamb(char c)
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

static char *
hunt(void)
{
	char c;
	if (gapsize() == bufsize)
		return buffer;
	draw();
	c = getch();
	return disamb(c);
}

static void
drawlinelbls(int lvl, int off)
{
	erase();
	move(0, 0);
	winbounds();
	for (char *i = start; i != end; i++)
		pc(*i);
	int count = 0;
	int step = skips(lvl) + 1;
	step = step < 1 ? 1 : step;
	for (int line = off; line < LINES; line += step) {
		move(line, 0);
		ptarg(count++);
	}
	refresh();
}

static bool
lineselected(int lvl, int off)
{
	return off + skips(lvl) > LINES;
}

static int
getoffset(int lvl, int off)
{
	char c = getch();
	int i = c - 'a';
	if (i < 0 || i >= 26)
		return -1;
	int delta = (skips(lvl) + 1) * i;
	return off + delta;
}

static char *
startofline(int off)
{
	char *result = start;
	while (off > 0) {
		if (result >= end)
			return NULL;
		if (*result == '\n')
			off--;
		result++;
	}
	return result;
}

static char *
endofline(int off)
{
	char *nextstart = startofline(off + 1);
	return nextstart - 1;
}

static int
linehunt(void)
{
	int lvl = 0;
	int off = 0;
	if (gapsize() == bufsize)
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

static void
rubout(char *t)
{
	int sz;
	sz = gap - (t + 1 - buffer);
	memmove(t, t + 1, sz);
	gap--;
}

static int
insertmode(char *t)
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
			if (t <= buffer)
				continue;
			t--;
			rubout(t);
			continue;
		}
		if (!isgraph(c) && !isspace(c)) {
			continue;
		}
		if (!bins(c, t))
			return 0;
		else
			t++;
	}
	return 1;
}

static void
delete(char *start, char *end)
{
	int n, tn;
	if (end != buffer + bufsize)
		end++;
	n = buffer + bufsize - end;
	tn = end - start;
	memmove(start, end, n);
	gap -= tn;
}

enum loopsig {
	sigcont,
	sigquit,
	sigerror
};

static enum loopsig
checksig(bool ok)
{
	return ok ? sigcont : sigerror;
}

typedef enum loopsig (*command_fn)(void);

static enum loopsig
scrolldown(void)
{
	doscrl(LINES / 2);
	return sigcont;
}

static enum loopsig
scrollup(void)
{
	doscrl(-LINES / 2);
	return sigcont;
}

static enum loopsig
quitcmd(void)
{
	return sigquit;
}

static enum loopsig
insertcmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return false;
	return checksig(insertmode(start));
}

static enum loopsig
appendcmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return false;
	if (start != buffer + bufsize)
		start++;
	return checksig(insertmode(start));
}

static enum loopsig
writecmd(void)
{
	return checksig(bsave());
}

static void
orient(char **start, char **end)
{
	if (*end < *start) {
		char *tmp = *end;
		*end = *start;
		*start = tmp;
	}
}

static enum loopsig
deletecmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return sigcont;
	char *end = hunt();
	if (end == NULL)
		return sigcont;
	orient(&start, &end);
	delete(start, end);
	return sigcont;
}

static enum loopsig
changecmd(void)
{
	char *start = hunt();
	if (start == NULL)
		return sigcont;
	char *end = hunt();
	if (end == NULL)
		return sigcont;
	orient(&start, &end);
	delete(start, end);
	return checksig(insertmode(start));
}

static enum loopsig
reloadcmd(void)
{
	breload();
	return sigcont;
}

static enum loopsig
jumptolinecmd(void)
{
	jumptoline();
	return sigcont;
}

static void
orienti(int *a, int *b)
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

static struct linerange
huntlinerange(void)
{
        int startoffset = linehunt();
        if (startoffset == -1)
                return (struct linerange){.start = NULL, .end = NULL};
        int endoffset = linehunt();
        if (endoffset == -1)
                return (struct linerange){.start = NULL, .end = NULL};
        orienti(&startoffset, &endoffset);
        char *start = startofline(startoffset);
        char *end = endofline(endoffset);
	return (struct linerange){.start = start, .end = end};
}

static enum loopsig
deletelinescmd(void)
{
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return sigcont;
	delete(r.start, r.end);
	return sigcont;
}

static enum loopsig
changelinescmd(void)
{
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return sigcont;
	delete(r.start, r.end);
	return checksig(insertmode(r.start));
}

static enum loopsig
lineoverlaycmd(void)
{
	int lineno = lwe_scroll + 1;
	int screenline = 0;
	attron(A_STANDOUT);
	while (screenline < LINES) {
		char nstr[32];
		snprintf(nstr, sizeof(nstr), "%4d", lineno);
		mvaddstr(screenline, 0, nstr);
		screenline++;
		lineno++;
	}
	attroff(A_STANDOUT);
	refresh();
	getch();
	return sigcont;
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
	['n'] = lineoverlaycmd
};

static int
cmdloop(void)
{
	for (;;) {
		draw();
		int c = getch();
		command_fn cmd = cmdtbl[c];
		if (cmd == NULL)
			continue;
		enum loopsig s = cmd();
		if (s == sigquit)
			return 1;
		else if (s == sigerror)
			return 0;
	}
	return 0;
}

static void
ed(void)
{
	if (!bread())
		return;
	lwe_scroll = 0;
	cmdloop();
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		err("missing file arg");
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
		if (strcmp(errbuf, ""))
			goto error;
		return 0;
	}
error:
	fprintf(stderr, "error: %s\n", errbuf);
	return 1;
}
