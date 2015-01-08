/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>

#define i int
#define c char
#define v void
#define str const char *
#define err(s) fprintf(stderr, "error: " s "\n")
#define p(s) printf("%s", s)
#define pc(c) printf("%c", c)

c *b, *scrl; i bs, g, gs, lines, cols;

i bext() { gs = bs; bs*=2; b=realloc(b, bs); if (b==NULL) return 0; return 1; }
i bput(c c_) { b[g++]=c_; gs--; if (gs==0) return bext(); return 1; }
i bread(str fn) {
   c c_; FILE *f; bs=4096; g = 0; gs = bs;
   b = malloc(bs); if (b==NULL) { err("memory"); return 0; }
   f = fopen(fn, "r"); if (f==NULL) { err("read"); return 0; }
   for (c_=fgetc(f);c_!=EOF;c_=fgetc(f)) if (!bput(c_)) return 0;
   return 1; }
i isend(c *s_) { return s_ >= (b + bs - gs); }
v draw() {
   i r_, c_; c *s_; r_ = c_ = 0; s_ = scrl; p("\x1B[2J\x1B[H");
   draw:
   if (isend(s_)) return;
   pc(*s_++); c_++; if (*s_=='\n') c_=0; c_%=cols; r_+=c_==0?1:0;
   if (r_ > lines) return; else goto draw; }
v scup() { /* TODO */ }
v scdown() {
   c *n_; n_ = scrl; loop: n_++;
   if (isend(n_) || *n_=='\n') {
      n_++;
      if (isend(n_)) { n_=b+bs-1; scrl=n_; scup();  }
      else { scrl=n_; }
   } else goto loop; }
v doscrl(i d_) {
   i i_;
   if (d_<0) for (i_=1;i_<-d_;i_++) scup(); else for (i_=1;i_<d_;i_++) scdown(); }
v cmdloop() { draw(); }
v ed(str fn_) { if (!bread(fn_)) return; scrl = b; doscrl(10); cmdloop(); }
v dtlines() {
   struct winsize w_; ioctl(0, TIOCGWINSZ, &w_);
   lines = w_.ws_row; cols = w_.ws_col; }
int main(i argc, c **argv) {
   dtlines();
   if (argc != 2) { err("missing file arg"); return 2; }
   else { ed(argv[1]); return 0; } }
