/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#define C_D 4
#define C_U 21
#define BKSPC 127

static char *fn, *b, *strt, *end;
static int bs, g, gs, lines, cols, scrl;
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
	b = realloc(b, bs);
	if (b == NULL) {
		err("memory");
		return 0;
	}
	return 1;
}

static int bput(char c_)
{
	b[g++] = c_;
	gs--;
	if (gs == 0)
		return bext();
	return 1;
}

static int bins(char c_, char *t_)
{
	int sz_;
	sz_ = g - (t_ - b);
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
	b = malloc(bs);
	if (b == NULL) {
		err("memory");
		return 0;
	}
	f_ = fopen(fn, "r");
	if (f_ == NULL) {
		fclose(f_);
		f_ = fopen(fn, "w");
		fclose(f_);
		f_ = fopen(fn, "r");
		if (f_ == NULL) {
			err("read");
			return 0;
		}
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
	FILE *f_ = fopen(fn, "w");
	if (f_ == NULL) {
		err("write");
		return 0;
	}
	fwrite(b, 1, bs - gs, f_);
	fclose(f_);
	return 1;
}

static int isend(const char *s_)
{
	return s_ >= (b + bs - gs);
}

static void cls(void)
{
	printf("\x1B[2J\x1B[H");
}

static void winbounds(void)
{
	int r_, c_, i_;
	r_ = c_ = 0;
	for (i_ = scrl, strt = b; i_ > 0 && !isend(strt); (strt)++)
		if (*strt == '\n')
			i_--;
	end = strt;
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
	printf("%c", c_);
}

static void draw(void)
{
	char *i_;
	cls();
	winbounds();
	for (i_ = strt; i_ != end; i_++)
		pc(*i_);
}

static void doscrl(int d_)
{
	scrl += d_;
	if (scrl < 0)
		scrl = 0;
}

static char *find(char c_, int n_)
{
	for (char *i_ = strt; i_ < end; i_++) {
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
	for (i_ = strt; i_ != end; i_++)
		if (*i_ == c_)
			ct_++;
	return ct_;
}

static void ptarg(int count_)
{
	char a_;
	a_ = 'a' + (count_ % 26);
	printf("\x1B[7m");
	pc(a_);
	printf("\x1B[0m");
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
	cls();
	ct_ = 0;
	for (i_ = strt; i_ < end; i_++) {
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
}

static char *disamb(char c_, int lvl_, int off_)
{
	if (count(c_) - off_ <= skips(lvl_))
		return find(c_, off_);
	drawdisamb(c_, lvl_, off_);
	char inp_;
	inp_ = getchar();
	int i_ = inp_ - 'a';
	if (i_ < 0 || i_ > 26)
		return 0;
	return disamb(c_, lvl_ + 1, off_ + i_ * skips(lvl_));
}

static char *hunt(void)
{
	if (gs == bs)
		return b;
	draw();
	char c_ = getchar();
	return disamb(c_, 0, 0);
}

static void rubout(char *t_)
{
	int sz_;
	sz_ = g - (t_ + 1 - b);
	memmove(t_, t_ + 1, sz_);
	gs++;
	g--;
}

static int insertmode(char *t_)
{
	char c_;
	for (;;) {
		draw();
		c_ = getchar();
		if (c_ == C_D)
			return 1;
		if (c_ == BKSPC) {
			if (t_ <= b)
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
	if (t_.e != b + bs)
		t_.e++;
	int n_ = b + bs - t_.e;
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
		c_ = getchar();
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
			if (t_.st != b + bs)
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
	scrl = 0;
	if (cmdloop())
		cls();
}

static void dtlines(void)
{
	struct winsize w_;
	ioctl(0, TIOCGWINSZ, &w_);
	lines = w_.ws_row;
	cols = w_.ws_col;
}

static void raw(void)
{
	system("stty cbreak -echo");
}

static void unraw(void)
{
	system("stty cooked echo");
}

int main(int argc, char **argv)
{
	dtlines();
	if (argc != 2) {
		err("missing file arg");
		return 2;
	} else {
		raw();
		fn = argv[1];
		ed();
		unraw();
		return 0;
	}
}
