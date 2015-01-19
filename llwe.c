/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "defines.h"

s c *b; s i bs, g, gs, lines, cols, scrl;
typedef struct { c *st, e; } tg;

s i bext() {
   gs = bs; bs*=2; b=realloc(b, bs);
   if (b==NULL) { err("memory"); return 0; } return 1; }
s i bput(c c_) { b[g++]=c_; gs--; if (gs==0) return bext(); return 1; }
s i bread(const c *fn) {
   c c_; FILE *f_; bs=4096; g = 0; gs = bs;
   b = malloc(bs); if (b==NULL) { err("memory"); return 0; }
   f_ = fopen(fn, "r"); if (f_==NULL) { err("read"); return 0; }
   for (c_=fgetc(f_);c_!=EOF;c_=fgetc(f_)) if (!bput(c_)) goto fail;
   fclose(f_); return 1; fail: fclose(f_); return 0; }
s i isend(const c *s_) { return s_ >= (b + bs - gs); }
s v cls() { p("\x1B[2J\x1B[H"); }
s v winbounds(c **s_, c **e_) {
   i r_, c_, i_; r_=c_=0;
   for (i_=scrl,*s_=b; i_>0 && !isend(*s_); (*s_)++) if (**s_=='\n') i_--;
   *e_=*s_; loop: if (isend(*e_)) return;
   c_++; if (**e_=='\n') { c_=0; } c_%=cols; if (c_==0) r_++; (*e_)++;
   if (r_<lines) goto loop; else (*e_)--; }
s v pc(char c_) { if (!isgraph(c_) && !isspace(c_)) c_='?'; printf("%c", c_); }
s v draw() {
   c *s_, *e_, *i_; winbounds(&s_, &e_);
   cls(); for (i_=s_; i_!=e_; i_++) pc(*i_); }
s v doscrl(i d_) { scrl+=d_; if (scrl < 0) scrl=0; }
s c *find(c c_, i n_) {
   c *s_, *e_, *i_; winbounds(&s_, &e_);
   for (i_=s_; i_!=e_; i_++) if (*i_==c_) n_--; if (n_<=0) return i_;
   return 0; }
s i count(c c_) {
   i ct_; c *i_, *s_, *e_; winbounds(&s_, &e_); ct_=0;
   for(i_=s_;i_!=e_;i_++) if (*i_==c_) ct_++; return ct_; }
s v ptarg(i count_) {
   c a_; a_='a'+(count_%26); p("\x1B[7m"); pc(a_); p("\x1B[0m"); }
s v drawdisamb(c c_, i lvl_, i off_) {
   i skip_, count_; c *s_, *e_, *i_; cls(); winbounds(&s_, &e_); count_=0;
   for (i_=s_; i_!=e_ && off_>0; i_++) { pc(*i_); if (*i_==c_) off_--; }
   for (;i_!=e_; i_++)
      if (*i_==c_) { ptarg(count_++); } else pc(*i_); }
s c *disamb(c c_, i lvl_, i off_) {
   drawdisamb(c_, lvl_, off_); c inp_; inp_=getchar();
   if (inp_<'a' || inp_>'z') return 0;
   if (count(c_) < 26*lvl_) return find(c_, inp_-'a');
   else return disamb(c_, lvl_+1, inp_-'a'); }
s c *hunt() {
   c c_; c_=getchar();
   if (count(c_)==1) return find(c_, 0); else return disamb(c_, 1, 0); }
s v cmdloop() {
   i q_; c c_; tg t_; for (q_=0;q_==0;) { draw(); c_=getchar();
   switch (c_) {
      case 4: doscrl(lines/2); break; case 21: doscrl(-lines/2); break;
      case 'q': case EOF: q_=1; break;
      case 'i': t_.st=hunt(); break; } } }
s v ed(const c *fn_) { if (!bread(fn_)) return; scrl=0; cmdloop(); cls(); }
s v dtlines() {
   struct winsize w_; ioctl(0, TIOCGWINSZ, &w_);
   lines = w_.ws_row; cols = w_.ws_col; }
s v raw() { system("stty cbreak -echo"); }
s v unraw() { system("stty cooked echo"); }
i main(i argc, c **argv) {
   dtlines();
   if (argc != 2) { err("missing file arg"); return 2; }
   else { raw(); ed(argv[1]); unraw(); return 0; } }
