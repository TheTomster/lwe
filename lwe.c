/* A unique cursorless text editor. (c) 2015 Tom Wright */
#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bang.h"
#include "buffer.h"
#include "err.h"
#include "draw.h"
#include "yank.h"
#include "undo.h"
#include "insert.h"

#define bufempty() (getbufstart() == getbufend())
#define screenline(n) (skipscreenlines(winstart(), n))
#define NULL_LINERANGE ((struct linerange) {.start = NULL, .end = NULL})

struct range {
	char *start;
	char *end;
};

/*
 * After a command executes, we signal whether to continue the main loop,
 * quit normally, or stop due to an error.
 */
enum loopsig {
	LOOP_SIGCNT,
	LOOP_SIGQUIT,
	LOOP_SIGERR
};

typedef enum loopsig (*command_fn) (void);

struct linerange {
	char *start;
	char *end;
};

struct yankstr {
	char *start;
	char *end;
};

#define C_D 4
#define C_U 21

static char *find(char c, int n);
static int disambget(int lvl, int off, int n);
static int lineselected(int lvl, int off);
static int getoffset(int lvl, int off);
static int huntline(void);
static void delete(char *start, char *end);
static enum loopsig scrolldown(void);
static enum loopsig scrollup(void);
static enum loopsig quitcmd(void);
static char *disamb(char c);
static char *hunt(void);
static enum loopsig insertcmd(void);
static enum loopsig appendcmd(void);
static enum loopsig writecmd(void);
static void orient(char **start, char **end);
static void huntrange(struct range *result);
static enum loopsig deletecmd(void);
static enum loopsig changecmd(void);
static int queryuser(char *out, int out_sz, char *prompt);
static enum loopsig jumptolinecmd(void);
static void orienti(int *a, int *b);
static char *bufline(char *start, int lines);
static struct linerange huntlinerange(void);
static enum loopsig deletelinescmd(void);
static enum loopsig changelinescmd(void);
static enum loopsig lineoverlaycmd(void);
static enum loopsig yankcmd(void);
static enum loopsig yanklinescmd(void);
static struct yankstr yankhunt(void);
static enum loopsig preputcmd(void);
static enum loopsig putcmd(void);
static enum loopsig insertlinecmd(void);
static enum loopsig appendlinecmd(void);
static int ranged_bang(char *start, char *end);
static enum loopsig bangcmd(void);
static enum loopsig banglinescmd(void);
static enum loopsig togglewhitespacecmd(void);
static enum loopsig undocmd(void);
static enum loopsig redocmd(void);
static enum loopsig preputlinecmd(void);
static enum loopsig putlinecmd(void);
static enum loopsig directionalsearch(int delta);
static enum loopsig searchcmd(void);
static enum loopsig rsearchcmd(void);
static int cmdloop(void);

static char *filename, *mode;
static char current_search[8192];

/* The list of all commands.  Unused entries will be NULL.  A character
 * can be used as the index into this array to look up the appropriate
 * command. */
static command_fn cmdtbl[512] = {
	[C_D] = scrolldown,
	[KEY_DOWN] = scrolldown,
	[KEY_NPAGE] = scrolldown,
	['j'] = scrolldown,
	[C_U] = scrollup,
	[KEY_UP] = scrollup,
	[KEY_PPAGE] = scrollup,
	['k'] = scrollup,
	['!'] = banglinescmd,
	['1'] = bangcmd,
	['A'] = appendlinecmd,
	['C'] = changelinescmd,
	['D'] = deletelinescmd,
	['I'] = insertlinecmd,
	['O'] = preputlinecmd,
	['P'] = putlinecmd,
	['Y'] = yanklinescmd,
	['a'] = appendcmd,
	['c'] = changecmd,
	['d'] = deletecmd,
	['g'] = jumptolinecmd,
	['i'] = insertcmd,
	['n'] = lineoverlaycmd,
	['o'] = preputcmd,
	['p'] = putcmd,
	['q'] = quitcmd,
	['r'] = redocmd,
	['s'] = togglewhitespacecmd,
	['u'] = undocmd,
	['w'] = writecmd,
	['y'] = yankcmd,
	['/'] = searchcmd,
	['?'] = rsearchcmd,
};

/* Finds the nth occurance of character c within the window.  Returns a
 * buffer pointer. */
static char *find(char c, int n)
{
	for (char *i = winstart(); i < winend(); i++) {
		if (*i != c) continue;
		if (n <= 0) return i;
		n--;
	}
	return NULL;
}

/* Takes a level, offset, and an index.  Finds the match at that index
 * for that level of disambiguation.  The index would correspond to the
 * key that the user pressed... 'c' => 2, 'a' => 0, for example. */
static int disambget(int lvl, int off, int n)
{
	int delta = (skips(lvl) + 1) * n;
	return off + delta;
}

/* Returns true if there is only one line on screen that matches the
 * given lvl and offset.  Since there could be more than 26 lines on
 * screen we have to perform disambiguation, and this function tells us
 * when to stop. */
static int lineselected(int lvl, int off)
{
	/* Checks if the 2nd match would be off screen. */
	return disambget(lvl, off, 1) > LINES;
}

/* Given a disamb level and offset (from previous layers of disamb),
 * queries the user for the next selection.  Reads a char from input
 * and calculates the offset for the next layer of disambiguation. */
static int getoffset(int lvl, int off)
{
	char c = getch();
	int i = c - 'a';
	if (i < 0 || i >= 26)
		return -1;
	return disambget(lvl, off, i);
}

/* Allow the user to select any line on the screen.  Returns the line
 * number (where the top of the screen is 0) that the user has selected.
 * This requires multiple layers of disambiguation of we have more than
 * 26 lines on screen. */
static int huntline(void)
{
	int lvl = 0;
	int off = 0;
	if (bufempty())
		return -1;
	while (!lineselected(lvl, off)) {
		clrscreen();
		drawtext();
		draw_eof();
		drawlinelbls(lvl, off);
		drawmodeline(filename, mode);
		present();
		off = getoffset(lvl, off);
		if (off < 0)
			return -1;
		lvl++;
	}
	return off;
}

/* Deletes some text from the buffer. */
static void delete(char *start, char *end)
{
	bufdelete(start, end);
	refresh_bounds();
}

static enum loopsig scrolldown(void)
{
	adjust_scroll(LINES / 2);
	return LOOP_SIGCNT;
}

static enum loopsig scrollup(void)
{
	adjust_scroll(-LINES / 2);
	return LOOP_SIGCNT;
}

static enum loopsig quitcmd(void)
{
	return LOOP_SIGQUIT;
}

/* In most cases when a user targets a character, there will be more than
 * one instance of that character visible on screen.  Disambiguation is
 * the process of refining the user's selection to the specific instance
 * that they are interested in.  We repeatedly ask for more input until
 * only one instance matches their inputs. */
static char *disamb(char c)
{
	int lvl = 0;
	int off = 0;
	while (!onlymatch(c, lvl, off)) {
		clrscreen();
		drawtext();
		draw_eof();
		drawdisamb(c, lvl, off);
		drawmodeline(filename, mode);
		present();
		off = getoffset(lvl, off);
		if (off < 0)
			return NULL;
		lvl++;
	}
	return find(c, off);
}

/* Starts off the process of choosing where an action should occur.  For an
 * empty buffer there's no choice but to start the buffer.  For all other
 * cases we ask the user for a character and start up the disambiguation
 * process. */
static char *hunt(void)
{
	char c;
	if (bufempty())
		return getbufstart();
	clrscreen();
	drawtext();
	draw_eof();
	drawmodeline(filename, mode);
	present();
	c = getch();
	if (!isgraph(c) && !isspace(c))
		return NULL;
	return disamb(c);
}

static enum loopsig insertcmd(void)
{
	char *t;
	mode = "TARGET (INSERT)";
	if (!(t = hunt()))
		return LOOP_SIGCNT;
	if (insertmode(filename, t) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

static enum loopsig appendcmd(void)
{
	char *t;
	mode = "TARGET (APPEND)";
	if (!(t = hunt()))
		return LOOP_SIGCNT;
	if (t != getbufend())
		t++;
	if (insertmode(filename, t) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

static enum loopsig writecmd(void)
{
	if (bufwrite(filename) < 0) {
		clrscreen();
		drawtext();
		draw_eof();
		drawmodeline(filename, mode);
		char messagebuf[256];
		snprintf(messagebuf, sizeof(messagebuf), "Error -- failed to write to file: %s", filename);
		drawmessage(messagebuf);
		present();
		getch();
	}
	return LOOP_SIGCNT;
}

/* We'll allow users to enter the start / end of ranged commands like delete
 * in either order.  orient flips the pointers so that the start always comes
 * before the end in the buffer. */
static void orient(char **start, char **end)
{
	if (*end < *start) {
		char *tmp = *end;
		*end = *start;
		*start = tmp;
	}
}

static void huntrange(struct range *result)
{
	result->start = hunt();
	if (result->start == NULL) {
		result->end = NULL;
		return;
	}
	result->end = hunt();
	if (result->end == NULL) {
		result->start = NULL;
		return;
	}
	orient(&result->start, &result->end);
	if (result->end != getbufend())
		result->end++;
	assert(result->start != NULL && result->end != NULL);
}

static enum loopsig deletecmd(void)
{
	mode = "TARGET (DELETE)";
	struct range r;
	huntrange(&r);
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	recstep();
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

static enum loopsig changecmd(void)
{
	struct range r;
	mode = "TARGET (CHANGE)";
	huntrange(&r);
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	delete(r.start, r.end);
	if (insertmode(filename, r.start) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

/* Queries the user for a string.  Returns 0 on succes, -1 if there
 * is a problem.  `out` is the buffer to store the user's response in.
 * `out_sz` is how much space is allocated for the user's response.
 * `prompt` is displayed to the user. */
static int queryuser(char *out, int out_sz, char *prompt)
{
	assert(out != NULL && out_sz > 0);
	assert(prompt != NULL);
	memset(out, '\0', out_sz);
	int plen = strlen(prompt);
	int i = 0;
	while (i < out_sz) {
		char msgbuf[COLS];
		snprintf(msgbuf, COLS, "%s: %.*s", prompt, COLS - plen - 2, out);
		clrscreen();
		drawtext();
		draw_eof();
		drawmessage(msgbuf);
		present();
		int c = getch();
		if (c == '\r') {
			return 0;
		} else if (c == C_D) {
			return -1;
		} else if (c == KEY_BACKSPACE || c == 127) {
			if (i <= 0)
				continue;
			i--;
			out[i] = '\0';
		} else if (!isgraph(c) && !isspace(c)) {
			continue;
		} else {
			out[i] = c;
			i++;
		}
	}
	clrscreen();
	drawtext();
	drawmessage("Input too long");
	getch();
	return -1;
}

static enum loopsig jumptolinecmd(void)
{
	mode = "JUMP";
	clrscreen();
	drawtext();
	draw_eof();
	drawmodeline(filename, mode);
	present();
	char buf[32];
	if (queryuser(buf, sizeof(buf), "JUMP") < 0)
		return LOOP_SIGCNT;
	int i = atoi(buf);
	if (i == 0)
		return LOOP_SIGCNT;
	set_scroll(i);
	adjust_scroll(-LINES / 2);
	return LOOP_SIGCNT;
}

/* orienti is similar to orient, but for integers.  This is useful for line
 * offsets. */
static void orienti(int *a, int *b)
{
	if (*b < *a) {
		int tmp = *a;
		*a = *b;
		*b = tmp;
	}
}

/* Skips lines in the buffer.  This looks at buffer lines, not screen lines. */
static char *bufline(char *start, int lines)
{
	for (int i = 0; i < lines; i++) {
		start = endofline(start);
		if (start != getbufend())
			start++;
	}
	return start;
}

/* Similar idea to hunt, but for a range of lines.  Pretty much just uses
 * huntline to get the start / end of the range.  Also guarantees the start
 * and end will be oriented properly, and returns NULL for both if there is a
 * problem with either. */
static struct linerange huntlinerange(void)
{
	int startoffset = huntline();
	if (startoffset == -1)
		return NULL_LINERANGE;
	int endoffset = huntline();
	if (endoffset == -1)
		return NULL_LINERANGE;
	orienti(&startoffset, &endoffset);
	char *start = bufline(winstart(), startoffset);
	if (start == NULL)
		return NULL_LINERANGE;
	char *lstart = bufline(winstart(), endoffset);
	if (lstart == NULL)
		return NULL_LINERANGE;
	char *end = endofline(lstart);
	if (end != getbufend())
		end++;
	return (struct linerange) {.start = start, .end = end};
}

static enum loopsig deletelinescmd(void)
{
	mode = "TARGET LINES (DELETE)";
	struct linerange r = huntlinerange();
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	recstep();
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

static enum loopsig changelinescmd(void)
{
	struct linerange r;
	mode = "TARGET LINES (CHANGE)";
	r = huntlinerange();
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	delete(r.start, r.end);
	if (insertmode(filename, r.start) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

/* Draws line numbers on the screen until dismissed with a key press. */
static enum loopsig lineoverlaycmd(void)
{
	clrscreen();
	drawtext();
	draw_eof();
	drawmodeline(filename, mode);
	drawlineoverlay();
	present();
	getch();
	return LOOP_SIGCNT;
}

static enum loopsig yankcmd(void)
{
	mode = "TARGET (YANK)";
	struct range r;
	huntrange(&r);
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	return LOOP_SIGCNT;
}

static enum loopsig yanklinescmd(void)
{
	mode = "TARGET LINES (YANK)";
	struct linerange r = huntlinerange();
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	saveyanks();
	return LOOP_SIGCNT;
}

/* Presents a menu for deciding which yanked string to use. */
static struct yankstr yankhunt(void)
{
	loadyanks();
	clrscreen();
	drawyanks();
	present();
	int selected = getch() - 'a';
	if (selected < 0 || selected >= yank_sz())
		return (struct yankstr) {NULL, NULL};
	struct yankstr result;
	unsigned ysz;
	yank_item(&result.start, &ysz, selected);
	result.end = result.start + ysz;
	return result;
}

static enum loopsig preputcmd(void)
{
	mode = "TARGET (PRE-PUT)";
	char *t = hunt();
	int ysz;
	if (t == NULL)
		return LOOP_SIGCNT;
	struct yankstr y = yankhunt();
	if (!y.start || !y.end)
		return LOOP_SIGCNT;
	if (!(t = bufinsertstr(y.start, y.end, t)))
		return LOOP_SIGERR;
	ysz = y.end - y.start;
	assert(ysz > 0);
	if (recinsert(t, t + ysz) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig putcmd(void)
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
	if (!(t = bufinsertstr(y.start, y.end, t)))
		return LOOP_SIGERR;
	int ysz = y.end - y.start;
	assert(ysz > 0);
	if (recinsert(t, t + ysz) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig insertlinecmd(void)
{
	int lineno;
	char *t;
	mode = "TARGET (INSERT)";
	if (bufempty())
		lineno = 0;
	else
		lineno = huntline();
	if (lineno == -1)
		return LOOP_SIGCNT;
	if(!(t = bufline(winstart(), lineno)))
		return LOOP_SIGCNT;
	if (!(t = bufinsert('\n', t)))
		return LOOP_SIGERR;
	if (recinsert(t, t + 1) < 0)
		return LOOP_SIGERR;
	if (insertmode(filename, t) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

static enum loopsig appendlinecmd(void)
{
	int lineno;
	char *lns, *lne;
	mode = "TARGET (APPEND)";
	if (bufempty())
		lineno = 0;
	else
		lineno = huntline();
	if (lineno == -1)
		return LOOP_SIGCNT;
	lns = bufline(winstart(), lineno);
	if(lns == NULL)
		return LOOP_SIGCNT;
	lne = endofline(lns);
	if (!(lne = bufinsert('\n', lne)))
		return LOOP_SIGERR;
	if (recinsert(lne, lne + 1) < 0)
		return LOOP_SIGERR;
	if (insertmode(filename, lne + 1) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

static int ranged_bang(char *start, char *end)
{
	char cmd[8192];
	int err;
	struct bang_output o;
	struct bang_output e;
	if (queryuser(cmd, sizeof(cmd), "COMMAND") < 0) {
		return 0;
	}
	if (bang(&o, &e, cmd, start, end - start) < 0) {
		clrscreen();
		drawmessage(e.buf);
		present();
		getch();
		err = 0;
		goto cleanup;
	}
	err = 0;
	yank_store(start, end);
	saveyanks();
	if (recdelete(start, end) < 0) {
		err = -1;
		goto cleanup;
	}
	bufdelete(start, end);
	if (!(start = bufinsertstr(o.buf, o.buf + o.sz, start))) {
		err = -1;
		goto cleanup;
	}
	if (recinsert(start, start + o.sz) < 0) {
		err = -1;
		goto cleanup;
	}
	recstep();
cleanup:
	free(o.buf);
	free(e.buf);
	refresh_bounds();
	return err;
}

static enum loopsig bangcmd(void)
{
	mode = "TARGET (SHELL)";
	struct range r;
	huntrange(&r);
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	if (ranged_bang(r.start, r.end) < 0)
		return LOOP_SIGERR;
	else
		return LOOP_SIGCNT;
}

static enum loopsig banglinescmd(void)
{
	mode = "TARGET (SHELL)";
	struct linerange r = huntlinerange();
	if (!r.start || !r.end)
		return LOOP_SIGCNT;
	if (ranged_bang(r.start, r.end) < 0)
		return LOOP_SIGERR;
	else
		return LOOP_SIGCNT;
}

static enum loopsig togglewhitespacecmd(void)
{
	show_whitespace = show_whitespace ^ 1;
	return LOOP_SIGCNT;
}

static enum loopsig undocmd(void)
{
	if (undo() < 0)
		return LOOP_SIGERR;
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig redocmd(void)
{
	if (redo() < 0)
		return LOOP_SIGERR;
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig preputlinecmd(void)
{
	struct yankstr y;
	char *t;
	int l, ysz;
	mode = "TARGET LINES (PRE-PUT)";
	l = huntline();
	if (l < 0)
		return LOOP_SIGCNT;
	if (!(t = bufline(winstart(), l)))
		return LOOP_SIGCNT;
	y = yankhunt();
	if (!y.start || !y.end)
		return LOOP_SIGCNT;
	if (!(t = bufinsertstr(y.start, y.end, t)))
		return LOOP_SIGERR;
	ysz = y.end - y.start;
	assert(ysz > 0);
	if (recinsert(t, t + ysz) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig putlinecmd(void)
{
	struct yankstr y;
	char *t;
	int l, ysz;
	mode = "TARGET LINES (PUT)";
	l = huntline();
	if (l < 0)
		return LOOP_SIGCNT;
	if (!(t = bufline(winstart(), l)))
		return LOOP_SIGCNT;
	t = endofline(t);
	if (t != getbufend())
		t++;
	y = yankhunt();
	if (!y.start || !y.end)
		return LOOP_SIGCNT;
	if (!(t = bufinsertstr(y.start, y.end, t)))
		return LOOP_SIGERR;
	ysz = y.end - y.start;
	assert(ysz > 0);
	if (recinsert(t, t + ysz) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
}

static enum loopsig directionalsearch(int delta)
{
	char rebuf[8192];
	regex_t reg;
	char *cpos, *spos, *e, *i;
	int err, n;
	if (queryuser(rebuf, sizeof(rebuf), "/") < 0)
		return LOOP_SIGCNT;
	if (rebuf[0] != '\0')
		snprintf(current_search, sizeof(current_search), "%s", rebuf);
	if ((err = regcomp(&reg, current_search, REG_EXTENDED)) != 0) {
		regerror(err, &reg, current_search, sizeof(current_search));
		clrscreen();
		drawtext();
		draw_eof();
		drawmessage(current_search);
		present();
		getch();
		return LOOP_SIGCNT;
	}
	spos = winstart();
	if (delta > 0) {
		cpos = endofline(spos);
		if (cpos >= getbufend())
			cpos = getbufstart();
	} else {
		if (spos == getbufstart())
			cpos = getbufend();
		else
			cpos = spos - 1;
	}
	while (cpos != spos) {
		e = endofline(cpos);
		if (*e == '\n') {
			*e = '\0';
			err = regexec(&reg, cpos, 0, NULL, 0);
			*e = '\n';
		} else {
			err = regexec(&reg, cpos, 0, NULL, 0);
		}
		if (!err)
			break;
		if (delta > 0) {
			cpos = e + 1;
			if (cpos >= getbufend())
				cpos = getbufstart();
		} else {
			if (cpos == getbufstart())
				cpos = getbufend();
			else
				cpos--;
		}
	}
	regfree(&reg);
	n = 0;
	for (i = getbufstart(); i < cpos; ++i)
		if (*i == '\n') n++;
	set_scroll(n);
	return LOOP_SIGCNT;
}

static enum loopsig searchcmd(void)
{
	return directionalsearch(1);
}

static enum loopsig rsearchcmd(void)
{
	return directionalsearch(-1);
}

static int cmdloop(void)
{
	set_scroll(0);
	for (;;) {
		mode = "COMMAND";
		clrscreen();
		drawtext();
		draw_eof();
		drawmodeline(filename, mode);
		present();
		int c = getch();
		if (c == ERR)
			continue;
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

int main(int argc, char **argv)
{
	initcurses();

	if (argc != 2) {
		seterr("missing file arg");
	} else {
		filename = argv[1];
		loadyanks();
		if (bufread(filename) == 0)
			cmdloop();
	}

	endwin();

	char errbuf[256];
	geterr(errbuf, sizeof(errbuf));
	if (errbuf[0] == '\0') {
		return 0;
	} else {
		fprintf(stderr, "error: %s\n", errbuf);
		return 1;
	}
}
