/* A unique cursorless text editor. (c) 2015 Tom Wright */
#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bang.h"
#include "buffer.h"
#include "err.h"
#include "draw.h"
#include "yank.h"
#include "undo.h"

#define KEY_ESCAPE 27
#define C_D 4
#define C_U 21
#define C_W 23

struct range {
	char *start;
	char *end;
};

char *filename, *mode;

#define bufempty() (getbufstart() == getbufend())
#define screenline(n) (skipscreenlines(winstart(), n))

/* Finds the nth occurance of character c within the window.  Returns a
 * buffer pointer. */
char *find(char c, int n)
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
int disambget(int lvl, int off, int n)
{
	int delta = (skips(lvl) + 1) * n;
	return off + delta;
}

/* Returns true if there is only one line on screen that matches the
 * given lvl and offset.  Since there could be more than 26 lines on
 * screen we have to perform disambiguation, and this function tells us
 * when to stop. */
bool lineselected(int lvl, int off)
{
	/* Checks if the 2nd match would be off screen. */
	return disambget(lvl, off, 1) > LINES;
}

/* Given a disamb level and offset (from previous layers of disamb),
 * queries the user for the next selection.  Reads a char from input
 * and calculates the offset for the next layer of disambiguation. */
int getoffset(int lvl, int off)
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
int huntline(void)
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

/* Deletes some text from the buffer, storing it in a new yank entry. */
void delete(char *start, char *end)
{
	bufdelete(start, end);
	refresh_bounds();
}

/* Deletes a word.  `t` is a pointer to a buffer pointer.  The pointer will
 * be moved back to before the previous word, and delete will be called to
 * remove the word from the buffer. */
void ruboutword(char **t)
{
	/* Don't delete letter that our cursor is on otherwise we
	 * would remove the letter after our last entered character
	 * in insert mode. */
	char *dend = *t;
	char *dstart = dend;
	while (isspace(*dstart) && (dstart > getbufstart()))
		dstart--;
	while (!isspace(*dstart) && (dstart > getbufstart()))
		dstart--;
	/* Preserve space before cursor when we can, it looks better. */
	if (dstart != getbufstart() && ((dstart + 1) < dend))
		dstart++;
	delete(dstart, dend);
	*t = dstart;
}

/*
 * Reads user input and updates the buffer / screen while the user is
 * inserting text.  Returns a pointer past the end of the inserted text,
 * or NULL if there is an error.
 */
char *insertmode(char *t)
{
	mode = "INSERT";
	int c;
	for (;;) {
		refresh_bounds();
		if (t > winend())
			adjust_scroll(LINES / 2);
		clrscreen();
		drawtext();
		draw_eof();
		drawmodeline(filename, mode);
		movecursor(t);
		present();
		c = getch();
		if (c == '\r')
			c = '\n';
		if (c == C_D || c == KEY_ESCAPE)
			return t;
		if (c == KEY_BACKSPACE || c == 127) {
			if (t <= getbufstart())
				continue;
			t--;
			bufdelete(t, t + 1);
			continue;
		}
		if (c == C_W) {
			if (t <= getbufstart())
				continue;
			t--;
			ruboutword(&t);
			continue;
		}
		if (!isgraph(c) && !isspace(c)) {
			continue;
		}
		if (!bufinsert(c, t))
			return NULL;
		else
			t++;
	}
	return t;
}

/* After a command executes, we signal whether to continue the main loop,
 * quit normally, or stop due to an error. */
enum loopsig {
	LOOP_SIGCNT,
	LOOP_SIGQUIT,
	LOOP_SIGERR
};

/* Convenience function for checking a boolean result.  On true we continue
 * the main loop, on false we error. */
enum loopsig checksig(bool ok)
{
	return ok ? LOOP_SIGCNT : LOOP_SIGERR;
}

typedef enum loopsig (*command_fn) (void);

enum loopsig scrolldown(void)
{
	adjust_scroll(LINES / 2);
	return LOOP_SIGCNT;
}

enum loopsig scrollup(void)
{
	adjust_scroll(-LINES / 2);
	return LOOP_SIGCNT;
}

enum loopsig quitcmd(void)
{
	return LOOP_SIGQUIT;
}

/* In most cases when a user targets a character, there will be more than
 * one instance of that character visible on screen.  Disambiguation is
 * the process of refining the user's selection to the specific instance
 * that they are interested in.  We repeatedly ask for more input until
 * only one instance matches their inputs. */
char *disamb(char c)
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
char *hunt(void)
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
	return disamb(c);
}

enum loopsig insertcmd(void)
{
	char *start, *end;
	mode = "TARGET (INSERT)";
	if (!(start = hunt()))
		return LOOP_SIGERR;
	if (!(end = insertmode(start)))
		return LOOP_SIGERR;
	if (recinsert(start, end) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

enum loopsig appendcmd(void)
{
	char *start, *end;
	mode = "TARGET (APPEND)";
	if (!(start = hunt()))
		return LOOP_SIGERR;
	if (start != getbufend())
		start++;
	if (!(end = insertmode(start)))
		return LOOP_SIGERR;
	if (recinsert(start, end) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

enum loopsig writecmd(void)
{
	if (!bufwrite(filename)) {
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
void orient(char **start, char **end)
{
	if (*end < *start) {
		char *tmp = *end;
		*end = *start;
		*start = tmp;
	}
}

void huntrange(struct range *result)
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

enum loopsig deletecmd(void)
{
	mode = "TARGET (DELETE)";
	struct range r;
	huntrange(&r);
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	recstep();
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

enum loopsig changecmd(void)
{
	struct range r;
	char *iend;
	mode = "TARGET (CHANGE)";
	huntrange(&r);
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	delete(r.start, r.end);
	if (!(iend = insertmode(r.start)))
		return LOOP_SIGERR;
	if (recinsert(r.start, iend) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

enum loopsig reloadcmd(void)
{
	bool ok = bufread(filename);
	if (!ok)
		return LOOP_SIGERR;
	refresh_bounds();
	return LOOP_SIGCNT;
}

/* Queries the user for a string.  Returns true on success, false if there
 * is a problem.  `out` is the buffer to store the user's response in.
 * `out_sz` is how much space is allocated for the user's response.
 * `prompt` is displayed to the user. */
bool queryuser(char *out, int out_sz, char *prompt)
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
		drawmessage(msgbuf);
		present();
		int c = getch();
		if (c == '\r') {
			return true;
		} else if (c == KEY_BACKSPACE || c == 127) {
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
	return false;
}

enum loopsig jumptolinecmd(void)
{
	mode = "JUMP";
	clrscreen();
	drawtext();
	draw_eof();
	drawmodeline(filename, mode);
	present();
	char buf[32];
	queryuser(buf, sizeof(buf), "JUMP");
	int i = atoi(buf);
	if (i == 0)
		return LOOP_SIGCNT;
	set_scroll(i);
	adjust_scroll(-LINES / 2);
	return LOOP_SIGCNT;
}

/* orienti is similar to orient, but for integers.  This is useful for line
 * offsets. */
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

#define NULL_LINERANGE ((struct linerange) {.start = NULL, .end = NULL})

/* Skips lines in the buffer.  This looks at buffer lines, not screen lines. */
char *bufline(char *start, int lines)
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
struct linerange huntlinerange(void)
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

enum loopsig deletelinescmd(void)
{
	mode = "TARGET LINES (DELETE)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	recstep();
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

enum loopsig changelinescmd(void)
{
	struct linerange r;
	char *iend;
	mode = "TARGET LINES (CHANGE)";
	r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	if (recdelete(r.start, r.end) < 0)
		return LOOP_SIGERR;
	delete(r.start, r.end);
	if (!(iend = insertmode(r.start)))
		return LOOP_SIGERR;
	if (recinsert(r.start, iend) < 0)
		return LOOP_SIGERR;
	return LOOP_SIGCNT;
}

/* Draws line numbers on the screen until dismissed with a key press. */
enum loopsig lineoverlaycmd(void)
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

enum loopsig yankcmd(void)
{
	mode = "TARGET (YANK)";
	struct range r;
	huntrange(&r);
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	return LOOP_SIGCNT;
}

enum loopsig yanklinescmd(void)
{
	mode = "TARGET LINES (YANK)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank_store(r.start, r.end);
	return LOOP_SIGCNT;
}

struct yankstr {
	char *start;
	char *end;
};

/* Presents a menu for deciding which yanked string to use. */
struct yankstr yankhunt(void)
{
	clrscreen();
	drawyanks();
	present();
	int selected = getch() - 'a';
	if (selected < 0 || selected >= yank_sz())
		return (struct yankstr) {NULL, NULL};
	struct yankstr result;
	int ysz;
	yank_item(&result.start, &ysz, selected);
	result.end = result.start + ysz;
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
	bool ok = bufinsertstr(y.start, y.end, t);
	if (!ok)
		return LOOP_SIGERR;
	if (recinsert(y.start, y.end) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
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
	bool ok = bufinsertstr(y.start, y.end, t);
	if (!ok)
		return LOOP_SIGERR;
	if (recinsert(y.start, y.end) < 0)
		return LOOP_SIGERR;
	recstep();
	refresh_bounds();
	return LOOP_SIGCNT;
}

enum loopsig insertlinecmd(void)
{
	int lineno;
	char *start, *end, *insertpos;
	mode = "TARGET (INSERT)";
	lineno = huntline();
	if (lineno == -1)
		return LOOP_SIGCNT;
	if(!(start = bufline(winstart(), lineno)))
		return LOOP_SIGCNT;
	insertpos = start;
	if (insertpos != getbufstart())
		insertpos--;
	bufinsert('\n', insertpos);
	if (!(end = insertmode(start)))
		return LOOP_SIGERR;
	if (recinsert(insertpos, end) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

enum loopsig appendlinecmd(void)
{
	int lineno;
	char *lns, *lne, *ie;
	mode = "TARGET (APPEND)";
	lineno = huntline();
	if (lineno == -1)
		return LOOP_SIGCNT;
	lns = bufline(winstart(), lineno);
	if(lns == NULL)
		return LOOP_SIGCNT;
	lne = endofline(lns);
	bufinsert('\n', lne);
	if (!(ie = insertmode(lne + 1)))
		return LOOP_SIGERR;
	if (recinsert(lne + 1, ie) < 0)
		return LOOP_SIGERR;
	recstep();
	return LOOP_SIGCNT;
}

bool ranged_bang(char *start, char *end)
{
	char cmd[8192];
	bool ok;
	ok = queryuser(cmd, sizeof(cmd), "COMMAND");
	if (!ok) {
		return true;
	}
	struct bang_output o;
	struct bang_output e;
	ok = bang(&o, &e, cmd, start, end - start);
	if (!ok) {
		clrscreen();
		drawmessage(e.buf);
		present();
		getch();
		goto cleanup;
	}
	yank_store(start, end);
	if (recdelete(start, end) < 0) {
		ok = false;
		goto cleanup;
	}
	bufdelete(start, end);
	bufinsertstr(o.buf, o.buf + o.sz, start);
	if (recinsert(start, start + o.sz) < 0) {
		ok = false;
		goto cleanup;
	}
	recstep();
cleanup:
	free(o.buf);
	free(e.buf);
	refresh_bounds();
	return ok;
}

enum loopsig bangcmd(void)
{
	mode = "TARGET (SHELL)";
	struct range r;
	huntrange(&r);
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	return checksig(ranged_bang(r.start, r.end));
}

enum loopsig banglinescmd(void)
{
	mode = "TARGET (SHELL)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	return checksig(ranged_bang(r.start, r.end));
}

enum loopsig togglewhitespacecmd(void)
{
	show_whitespace = show_whitespace ^ 1;
	return LOOP_SIGCNT;
}

/* The list of all commands.  Unused entries will be NULL.  A character
 * can be used as the index into this array to look up the appropriate
 * command. */
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
	['I'] = insertlinecmd,
	['a'] = appendcmd,
	['A'] = appendlinecmd,
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
	['o'] = preputcmd,
	['1'] = bangcmd,
	['!'] = banglinescmd,
	['s'] = togglewhitespacecmd,
};

int cmdloop(void)
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
		if (bufread(filename))
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
