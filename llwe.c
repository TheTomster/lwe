/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "defines.h"

s c *b; s i bs, g, gs, lines, cols, scrl;
typedef struct { i st, e; } tg;

s i bext() {
   gs = bs; bs*=2; b=realloc(b, bs);
   if (b==NULL) { err("memory"); return 0; } return 1; }
s i bput(c c_) { b[g++]=c_; gs--; if (gs==0) return bext(); return 1; }
s i bread(str fn) {
   c c_; FILE *f_; bs=4096; g = 0; gs = bs;
   b = malloc(bs); if (b==NULL) { err("memory"); return 0; }
   f_ = fopen(fn, "r"); if (f_==NULL) { err("read"); return 0; }
   for (c_=fgetc(f_);c_!=EOF;c_=fgetc(f_)) if (!bput(c_)) goto fail;
   fclose(f_); return 1; fail: fclose(f_); return 0; }
s i isend(const c *s_) { return s_ >= (b + bs - gs); }
s v cls() { p("\x1B[2J\x1B[H"); }
s v winbounds(str *s_, str *e_) {
   i r_, c_, i_; r_=c_=0;
   for (i_=scrl,*s_=b; i_>0 && !isend(*s_); (*s_)++) if (**s_=='\n') i_--;
   *e_=*s_; loop: if (isend(*e_)) return;
   c_++; if (**e_=='\n') { c_=0; } c_%=cols; if (c_==0) r_++; (*e_)++;
   if (r_<=lines) goto loop; }
s v pc(char c_) { if (!isgraph(c_) && !isspace(c_)) c_='?'; printf("%c", c_); }
s v draw() {
   str s_; str e_; str i_; winbounds(&s_, &e_);
   cls(); for (i_=s_; i_!=e_; i_++) {
      pc(*i_); if (*i_=='\n') pc('\r'); } }
s v doscrl(i d_) { scrl+=d_; if (scrl < 0) scrl=0; }
s tg trgt() { tg r; r.st=0; r.e=0; return r; }
s v cmdloop() {
   i q_; c c_; for (q_=0;q_==0;) { draw(); c_=getchar();
   switch (c_) {
      case 4: doscrl(lines/2); break; case 21: doscrl(-lines/2); break;
      case 'q': case EOF: q_=1; break; } } }
s v ed(str fn_) { if (!bread(fn_)) return; scrl=0; cmdloop(); cls(); }
s v dtlines() {
   struct winsize w_; ioctl(0, TIOCGWINSZ, &w_);
   lines = w_.ws_row; cols = w_.ws_col; }
s v raw() { system("stty cbreak -echo"); }
s v unraw() { system("stty cooked echo"); }
i main(i argc, c **argv) {
   dtlines();
   if (argc != 2) { err("missing file arg"); return 2; }
   else { raw(); ed(argv[1]); unraw(); return 0; } }
