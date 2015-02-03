/* A unique cursorless text editor. (c) 2015 Tom Wright */
#include <ctype.h>
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_D 4
#define C_U 21

static char *filename, *buffer, *start, *end;
char errbuf[256];
static int bufsize, gap, gapsize, lwe_scroll;

static void
err(const char *str)
{
	snprintf(errbuf, sizeof(errbuf), "%s", str);
}

static int
bext(void)
{
	gapsize = bufsize;
	bufsize *= 2;
	buffer = realloc(buffer, bufsize);
	if (buffer == NULL) {
		err("memory");
		return 0;
	}
	return 1;
}

static int
bput(char c)
{
	buffer[gap++] = c;
	gapsize--;
	if (gapsize == 0)
		return bext();
	return 1;
}

static int
bins(char c, char *t)
{
	int sz;
	sz = gap - (t - buffer);
	memmove(t + 1, t, sz);
	*t = c;
	gap++;
	gapsize--;
	if (gapsize == 0)
		return bext();
	else
		return 1;
}

static int
bread(void)
{
	char c;
	FILE *f;
	bufsize = 4096;
	gap = 0;
	gapsize = bufsize;
	buffer = malloc(bufsize);
	if (buffer == NULL) {
		err("memory");
		return 0;
	}
	f = fopen(filename, "r");
	if (f == NULL) {
		return 1;
	}
	for (c = fgetc(f); c != EOF; c = fgetc(f))
		if (!bput(c))
			goto fail;
	fclose(f);
	return 1;
fail:	fclose(f);
	return 0;
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
	fwrite(buffer, 1, bufsize - gapsize, f);
	fclose(f);
	return 1;
}

static int
isend(const char *s)
{
	return s >= (buffer + bufsize - gapsize);
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
	int i;
	for (i = 1; lvl > 0; lvl--)
		i *= 26;
	return i;
}

static void
drawdisamb(char c, int lvl, int off)
{
	char *i;
	int ct;
	erase();
	move(0, 0);
	ct = 0;
	for (i = start; i < end; i++) {
		if (*i == c && off > 0) {
			pc(*i);
			off--;
		} else if (*i == c && off <= 0) {
			ptarg(ct++);
			off = skips(lvl) - 1;
		} else {
			pc(*i);
		}
	}
	refresh();
}

static char *
disamb(char c, int lvl, int off)
{
	char inp;
	int i;
	if (count(c) - off <= skips(lvl))
		return find(c, off);
	drawdisamb(c, lvl, off);
	inp = getch();
	i = inp - 'a';
	if (i < 0 || i > 26)
		return 0;
	return disamb(c, lvl + 1, off + i * skips(lvl));
}

static char *
hunt(void)
{
	char c;
	if (gapsize == bufsize)
		return buffer;
	draw();
	c = getch();
	return disamb(c, 0, 0);
}

static void
rubout(char *t)
{
	int sz;
	sz = gap - (t + 1 - buffer);
	memmove(t, t + 1, sz);
	gapsize++;
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
	gapsize += tn;
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
	char *end = hunt();
	if (start == NULL || end == NULL)
		return false;
	orient(&start, &end);
	delete(start, end);
	return sigcont;
}

static enum loopsig
changecmd(void)
{
	char *start = hunt();
	char *end = hunt();
	if (start == NULL || end == NULL)
		return false;
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
};

static int
cmdloop(void)
{
	for (;;) {
		draw();
		int c = getch();
		enum loopsig s = cmdtbl[c]();
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
