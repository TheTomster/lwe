/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "defines.h"

c *b; i bs, g, gs, lines, cols, scrl;

i bext() {
   gs = bs; bs*=2; b=realloc(b, bs);
   if (b==NULL) { err("memory"); return 0; } return 1; }
i bput(c c_) { b[g++]=c_; gs--; if (gs==0) return bext(); return 1; }
i bread(str fn) {
   c c_; FILE *f_; bs=4096; g = 0; gs = bs;
   b = malloc(bs); if (b==NULL) { err("memory"); return 0; }
   f_ = fopen(fn, "r"); if (f_==NULL) { err("read"); return 0; }
   for (c_=fgetc(f_);c_!=EOF;c_=fgetc(f_)) if (!bput(c_)) goto fail;
   fclose(f_); return 1; fail: fclose(f_); return 0; }
i isend(const c *s_) { return s_ >= (b + bs - gs); }
v cls() { p("\x1B[2J\x1B[H"); }
v draw() {
   i r_, c_, i_; c *s_;  r_=c_=0; cls();
   for (i_=scrl,s_=b; i_>0 && !isend(s_); s_++) if (*s_=='\n') i_--;
   draw: if (isend(s_)) return;
   pc(*s_++); c_++; if (*s_=='\n') { pc('\r'); c_=0; } c_%=cols; if (c_==0) r_++;
   if (r_ <= lines) goto draw; }
v doscrl(i d_) { scrl+=d_; if (scrl < 0) scrl=0; }
v cmdloop() {
   i q_; c c_; for (q_=0;q_==0;) { draw(); c_=getchar();
   switch (c_) {
      case 4: doscrl(lines/2); break; case 21: doscrl(-lines/2); break;
      case 'q': case EOF: q_=1; break; } } }
v ed(str fn_) { if (!bread(fn_)) return; scrl=0; cmdloop(); cls(); }
v dtlines() {
   struct winsize w_; ioctl(0, TIOCGWINSZ, &w_);
   lines = w_.ws_row; cols = w_.ws_col; }
v raw() { system("stty raw -echo"); }  v unraw() { system("stty cooked echo"); }
i main(i argc, c **argv) {
   dtlines();
   if (argc != 2) { err("missing file arg"); return 2; }
   else { raw(); ed(argv[1]); unraw(); return 0; } }
