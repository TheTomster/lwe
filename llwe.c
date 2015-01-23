/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <curses.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define C_D 4
#define C_U 21

static char *filename, *buffer, *start, *end;
static int bs, g, gs, lines, cols, lwe_scroll;
typedef struct {
	char *st, *e;
} tg;

static void err(const char *str)
{
	fprintf(stderr, "error: %s\n", str);
}

static int bext(void)
{
	gs = bs;
	bs *= 2;
	buffer = realloc(buffer, bs);
	if (buffer == NULL) {
		err("memory");
		return 0;
	}
	return 1;
}

static int bput(char c_)
{
	buffer[g++] = c_;
	gs--;
	if (gs == 0)
		return bext();
	return 1;
}

static int bins(char c_, char *t_)
{
	int sz_;
	sz_ = g - (t_ - buffer);
	memmove(t_ + 1, t_, sz_);
	*t_ = c_;
	g++;
	gs--;
	if (gs == 0)
		return bext();
	else
		return 1;
}

static int bread(void)
{
	char c_;
	FILE *f_;
	bs = 4096;
	g = 0;
	gs = bs;
	buffer = malloc(bs);
	if (buffer == NULL) {
		err("memory");
		return 0;
	}
	f_ = fopen(filename, "r");
	if (f_ == NULL) {
		return 1;
	}
	for (c_ = fgetc(f_); c_ != EOF; c_ = fgetc(f_))
		if (!bput(c_))
			goto fail;
	fclose(f_);
	return 1;
fail:	fclose(f_);
	return 0;
}

static int bsave(void)
{
	FILE *f_ = fopen(filename, "w");
	if (f_ == NULL) {
		err("write");
		return 0;
	}
	fwrite(buffer, 1, bs - gs, f_);
	fclose(f_);
	return 1;
}

static int isend(const char *s_)
{
	return s_ >= (buffer + bs - gs);
}

static void winbounds(void)
{
	int r_, c_, i_;
	r_ = c_ = 0;
	for (i_ = lwe_scroll, start = buffer; i_ > 0 && !isend(start);
	     (start)++)
		if (*start == '\n')
			i_--;
	end = start;
loop:	if (isend(end))
		return;
	c_++;
	if (*end == '\n') {
		c_ = 0;
	}
	c_ %= cols;
	if (c_ == 0)
		r_++;
	end++;
	if (r_ < lines)
		goto loop;
	else
		end--;
}

static void pc(char c_)
{
	if (!isgraph(c_) && !isspace(c_))
		c_ = '?';
	addch(c_);
}

static void draw(void)
{
	char *i_;
	erase();
	move(0, 0);
	winbounds();
	for (i_ = start; i_ != end; i_++)
		pc(*i_);
	refresh();
}

static void doscrl(int d_)
{
	lwe_scroll += d_;
	if (lwe_scroll < 0)
		lwe_scroll = 0;
}

static char *find(char c_, int n_)
{
	for (char *i_ = start; i_ < end; i_++) {
		if (*i_ == c_) {
			if (n_ <= 0)
				return i_;
			else
				n_--;
		}
	}
	return 0;
}

static int count(char c_)
{
	int ct_;
	char *i_;
	ct_ = 0;
	for (i_ = start; i_ != end; i_++)
		if (*i_ == c_)
			ct_++;
	return ct_;
}

static void ptarg(int count_)
{
	char a_;
	a_ = 'a' + (count_ % 26);
	attron(A_STANDOUT);
	pc(a_);
	attroff(A_STANDOUT);
}

static int skips(int lvl_)
{
	int i_;
	for (i_ = 1; lvl_ > 0; lvl_--)
		i_ *= 26;
	return i_;
}

static void drawdisamb(char c_, int lvl_, int off_)
{
	char *i_;
	int ct_;
	erase();
	move(0, 0);
	ct_ = 0;
	for (i_ = start; i_ < end; i_++) {
		if (*i_ == c_ && off_ > 0) {
			pc(*i_);
			off_--;
		} else if (*i_ == c_ && off_ <= 0) {
			ptarg(ct_++);
			off_ = skips(lvl_) - 1;
		} else {
			pc(*i_);
		}
	}
	refresh();
}

static char *disamb(char c_, int lvl_, int off_)
{
	if (count(c_) - off_ <= skips(lvl_))
		return find(c_, off_);
	drawdisamb(c_, lvl_, off_);
	char inp_;
	inp_ = getch();
	int i_ = inp_ - 'a';
	if (i_ < 0 || i_ > 26)
		return 0;
	return disamb(c_, lvl_ + 1, off_ + i_ * skips(lvl_));
}

static char *hunt(void)
{
	if (gs == bs)
		return buffer;
	draw();
	char c_ = getch();
	return disamb(c_, 0, 0);
}

static void rubout(char *t_)
{
	int sz_;
	sz_ = g - (t_ + 1 - buffer);
	memmove(t_, t_ + 1, sz_);
	gs++;
	g--;
}

static int insertmode(char *t_)
{
	int c_;
	for (;;) {
		draw();
		c_ = getch();
		if (c_ == '\r')
			c_ = '\n';
		if (c_ == C_D)
			return 1;
		if (c_ == KEY_BACKSPACE) {
			if (t_ <= buffer)
				continue;
			t_--;
			rubout(t_);
			continue;
		}
		if (!isgraph(c_) && !isspace(c_)) {
			continue;
		}
		if (!bins(c_, t_))
			return 0;
		else
			t_++;
	}
	return 1;
}

static void delete(tg t_)
{
	if (t_.e != buffer + bs)
		t_.e++;
	int n_ = buffer + bs - t_.e;
	int tn_ = t_.e - t_.st;
	memmove(t_.st, t_.e, n_);
	g -= tn_;
	gs += tn_;
}

static int cmdloop(void)
{
	int q_;
	char c_;
	tg t_;
	for (q_ = 0; q_ == 0;) {
		draw();
		c_ = getch();
		switch (c_) {
		case C_D:
			doscrl(lines / 2);
			break;
		case C_U:
			doscrl(-lines / 2);
			break;
		case 'q':
		case EOF:
			q_ = 1;
			break;
		case 'i':
			t_.st = hunt();
			if (t_.st == 0)
				break;
			if (!insertmode(t_.st))
				return 0;
			break;
		case 'a':
			t_.st = hunt();
			if (t_.st == 0)
				break;
			if (t_.st != buffer + bs)
				t_.st++;
			if (!insertmode(t_.st))
				return 0;
			break;
		case 'w':
			if (!bsave())
				return 0;
			break;
		case 'd':
			t_.st = hunt();
			t_.e = hunt();
			if (t_.st == 0 || t_.e == 0)
				break;
			delete(t_);
			break;
		case 'c':
			t_.st = hunt();
			t_.e = hunt();
			if (t_.st == 0 || t_.e == 0)
				break;
			delete(t_);
			insertmode(t_.st);
			break;
		}
	}
	return 1;
}

static void ed(void)
{
	if (!bread())
		return;
	lwe_scroll = 0;
	cmdloop();
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		err("missing file arg");
		return 2;
	} else {
		initscr();
		cbreak();
		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);
		getmaxyx(stdscr, lines, cols);
		filename = argv[1];
		ed();
		endwin();
		return 0;
	}
}
