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
#include "draw.h"

#define C_D 4
#define C_U 21
#define C_W 23

char *filename, *mode;
char *yanks[26];
int yanksizes[26];

#define bufempty() (getbufptr() == getbufend())
#define screenline(n) (skipscreenlines(winstart(), n))

/* Finds the nth occurance of character c within the window.  Returns a
 * buffer pointer. */
char *find(char c, int n)
{
	char *i;
	for (i = winstart(); i < winend(); i++) {
		if (*i == c) {
			if (n <= 0)
				return i;
			else
				n--;
		}
	}
	return 0;
}

/* Returns true if there is only one line on screen that matches the
 * given lvl and offset.  Since there could be more than 26 lines on
 * screen we have to perform disambiguation, and this function tells us
 * when to stop. */
bool lineselected(int lvl, int off)
{
	return off + skips(lvl) > LINES;
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
	int delta = (skips(lvl) + 1) * i;
	return off + delta;
}

/* Allow the user to select any line on the screen.  Returns the line
 * number (where the top of the screen is 0) that the user has selected.
 * This requires multiple layers of disambiguation of we have more than
 * 26 lines on screen. */
int linehunt(void)
{
	int lvl = 0;
	int off = 0;
	if (bufempty())
		return -1;
	while (!lineselected(lvl, off)) {
		drawlinelbls(lvl, off, filename, mode);
		off = getoffset(lvl, off);
		if (off < 0)
			return -1;
		lvl++;
	}
	return off;
}

/* Yanked text is stored in malloc'd buffers.  The yanks array is an
 * array of pointers to these buffers.  When a new item is yanked, we
 * want to shift everything down, dropping the last item in order to
 * make room for the new item. */
void shiftyanks(void)
{
	if (yanks[25] != NULL)
		free(yanks[25]);
	memmove(&yanks[1], &yanks[0], sizeof(yanks[0]) * 25);
	memmove(&yanksizes[1], &yanksizes[0], sizeof(yanksizes[0]) * 25);
}

/* Copies the text indicated by start and end into a new yank entry. */
void yank(char *start, char *end)
{
	if (end != getbufend())
		end++;
	shiftyanks();
	int sz = end - start;
	assert(sz >= 0);
	yanksizes[0] = sz;
	yanks[0] = malloc(sz);
	memcpy(yanks[0], start, sz);
}

/* Deletes some text from the buffer, storing it in a new yank entry. */
void delete(char *start, char *end)
{
	if (end != getbufend())
		end++;
	bufdelete(start, end);
}

/* Deletes a word.  `t` is a pointer to a buffer pointer.  The pointer will be
 * moved back to before the previous word, and delete will be called to remove
 * the word from the buffer. */
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

/* Moves the terminal cursor to point to the given buffer location on screen. */
void movecursor(char *t){
	if (!t) 
		return;
	// Figure out line number, number of tabs on the line,
	// along with how much space each tab is actually taking up
	// so we can figure out the offset in order to highlight the cursor
	int offset = 0;
	int linenumber = countwithin(winstart(), t, '\n');
	char* linestart = screenline(linenumber);
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

/* Reads user input and updates the buffer / screen while the user is
 * inserting text. */
int insertmode(char *t)
{
	mode = "INSERT";
	int c;
	for (;;) {
		old_draw(filename, mode);
		if (t > winend())
			adjust_scroll(LINES / 2);
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
	int toskip = 0;
	while (!onlymatch(c, lvl, toskip)) {
		drawdisamb(c, lvl, toskip, filename, mode);
		char input = getch();
		int i = input - 'a';
		if (i < 0 || i >= 26)
			return NULL;
		toskip += i * (skips(lvl) + 1);
		lvl++;
	}
	return find(c, toskip);
}

/* Starts off the process of choosing where an action should occur.  For an
 * empty buffer there's no choice but to start the buffer.  For all other
 * cases we ask the user for a character and start up the disambiguation
 * process. */
char *hunt(void)
{
	char c;
	if (bufempty())
		return getbufptr();
	old_draw(filename, mode);
	c = getch();
	return disamb(c);
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
	yank(start, end);
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
	yank(start, end);
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
	old_draw(filename, mode);
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

/* Similar idea to hunt, but for a range of lines.  Pretty much just uses
 * linehunt to get the start / end of the range.  Also guarantees the start
 * and end will be oriented properly, and returns NULL for both if there is a
 * problem with either. */
struct linerange huntlinerange(void)
{
	int startoffset = linehunt();
	if (startoffset == -1)
		goto retnull;
	int endoffset = linehunt();
	if (endoffset == -1)
		goto retnull;
	orienti(&startoffset, &endoffset);
	char *start = screenline(startoffset);
	if (start == NULL)
		goto retnull;
	char *lstart = screenline(endoffset);
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
	yank(r.start, r.end);
	delete(r.start, r.end);
	return LOOP_SIGCNT;
}

enum loopsig changelinescmd(void)
{
	mode = "TARGET LINES (CHANGE)";
	struct linerange r = huntlinerange();
	if (r.start == NULL || r.end == NULL)
		return LOOP_SIGCNT;
	yank(r.start, r.end);
	delete(r.start, r.end);
	return checksig(insertmode(r.start));
}

/* Draws line numbers on the screen until dismissed with a key press. */
enum loopsig lineoverlaycmd(void)
{
	int lineno = scroll_line() + 1;
	int screenline = 0;
	int fileline = 0;
	attron(A_STANDOUT);
	while (screenline < LINES) {
		char nstr[32];
		snprintf(nstr, sizeof(nstr), "%4d", lineno);
		mvaddstr(screenline, 0, nstr);
		char *lstart = screenline(fileline);
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

/* Presents a menu for deciding which yanked string to use. */
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
		old_draw(filename, mode);
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
