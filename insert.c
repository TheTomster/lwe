#include <curses.h>
#include <ctype.h>
#include <stdbool.h>

#include "buffer.h"
#include "draw.h"
#include "insert.h"
#include "undo.h"

#define C_D 4
#define C_W 23
#define KEY_ESCAPE 27

static int ruboutword(char **t);

/* Deletes a word.  `t` is a pointer to a buffer pointer.  The pointer will
 * be moved back to before the previous word, and delete will be called to
 * remove the word from the buffer. */
static int ruboutword(char **t)
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
	if (dend != getbufend())
		dend++;
	if (recdelete(dstart, dend) < 0)
		return -1;
	bufdelete(dstart, dend);
	refresh_bounds();
	*t = dstart;
	return 0;
}

/*
 * Reads user input and updates the buffer / screen while the user is
 * inserting text.  Returns a pointer past the end of the inserted text,
 * or NULL if there is an error.
 */
int insertmode(char *filename, char *t)
{
	int c;
	for (;;) {
		refresh_bounds();
		if (t > winend())
			adjust_scroll(winrows() / 2);
		clrscreen();
		drawtext();
		draw_eof();
		drawmodeline(filename, "INSERT");
		movecursor(t);
		present();
		c = getch();
		if (c == '\r')
			c = '\n';
		if (c == C_D || c == KEY_ESCAPE)
			break;
		if (c == KEY_BACKSPACE || c == 127) {
			if (t <= getbufstart())
				continue;
			t--;
			if (recdelete(t, t+1) < 0)
				return -1;
			bufdelete(t, t + 1);
			continue;
		}
		if (c == C_W) {
			if (t <= getbufstart())
				continue;
			t--;
			if (ruboutword(&t) < 0)
				return -1;
			continue;
		}
		if (!isgraph(c) && !isspace(c)) {
			continue;
		}
		if (!bufinsert(c, t))
			return -1;
		if (recinsert(t, t + 1) < 0)
			return -1;
		else
			t++;
	}
	return 0;
}
