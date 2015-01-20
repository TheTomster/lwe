/* Extra lite version of LWE (c) 2015 Tom Wright */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#define s static
#define i int
#define c char
#define v void
#define err(s) fprintf(stderr, "error: " s "\n");
#define p(s) printf("%s", s);
#define C_D 4
#define C_U 21
#define BKSPC 127

s c *fn, *b, *strt, *end; s i bs, g, gs, lines, cols, scrl;
typedef struct { c *st, *e; } tg;

s i bext(void) {
   gs = bs; bs*=2; b=realloc(b, bs);
   if (b==NULL) { err("memory"); return 0; } return 1; }
s i bput(c c_) { b[g++]=c_; gs--; if (gs==0) return bext(); return 1; }
s i bins(c c_, c *t_) {
   memmove(t_+1, t_, (b+bs)-t_); *t_=c_; g++; gs--;
   if (gs==0) return bext(); else return 1; }
s i bread(void) {
   c c_; FILE *f_; bs=4096; g=0; gs=bs;
   b=malloc(bs); if (b==NULL) { err("memory"); return 0; }
   f_=fopen(fn, "r"); if (f_==NULL) { err("read"); return 0; }
   for (c_=fgetc(f_);c_!=EOF;c_=fgetc(f_)) if (!bput(c_)) goto fail;
   fclose(f_); return 1; fail: fclose(f_); return 0; }
s i bsave(void) {
   FILE *f_=fopen(fn, "w"); if (f_==NULL) { err("write"); return 0; }
   fwrite(b, 1, bs-gs, f_); fclose(f_); return 1; }
s i isend(const c *s_) { return s_ >= (b + bs - gs); }
s v cls(void) { p("\x1B[2J\x1B[H"); }
s v winbounds(void) {
   i r_, c_, i_; r_=c_=0;
   for (i_=scrl,strt=b; i_>0 && !isend(strt); (strt)++) if (*strt=='\n') i_--;
   end=strt; loop: if (isend(end)) return;
   c_++; if (*end=='\n') { c_=0; } c_%=cols; if (c_==0) r_++; end++;
   if (r_<lines) goto loop; else end--; }
s v pc(char c_) { if (!isgraph(c_) && !isspace(c_)) c_='?'; printf("%c", c_); }
s v draw(void) {
   c *i_; cls(); winbounds(); for (i_=strt; i_!=end; i_++) pc(*i_); }
s v doscrl(i d_) { scrl+=d_; if (scrl < 0) scrl=0; }
s c *find(c c_, i n_) {
   for (c *i_=strt; i_<end; i_++) { if (*i_==c_) { if (n_<=0) return i_; else n_--; } }
   return 0; }
s i count(c c_) {
   i ct_; c *i_; ct_=0; for(i_=strt;i_!=end;i_++)
      if (*i_==c_) ct_++; return ct_; }
s v ptarg(i count_) {
   c a_; a_='a'+(count_%26); p("\x1B[7m"); pc(a_); p("\x1B[0m"); }
s i skips(i lvl_) { i i_; for(i_=1;lvl_>0;lvl_--) i_*=26; return i_; }
s v drawdisamb(c c_, i lvl_, i off_) {
   c *i_; i ct_; cls(); ct_=0; for (i_=strt; i_<end; i_++) {
      if (*i_==c_ && off_>0) { pc(*i_); off_--; }
      else if (*i_==c_ && off_<=0) { ptarg(ct_++); off_=skips(lvl_)-1; }
      else { pc(*i_); } } }
s c *disamb(c c_, i lvl_, i off_) {
   if (count(c_)-off_<=skips(lvl_)) return find(c_, off_);
   drawdisamb(c_, lvl_, off_); c inp_; inp_=getchar(); i i_=inp_-'a';
   return disamb(c_, lvl_+1, off_+i_*skips(lvl_)); }
s c *hunt(void) {
   if(gs==bs) return b; draw(); c c_=getchar(); return disamb(c_, 0, 0); }
s v rubout(c *t_) { memmove(t_, t_+1, (b+bs)-t_+1); gs++; g--; }
s i insertmode(c *t_) { c c_; for (;;) { draw(); c_=getchar();
   if (c_==C_D) return 1;
   if (c_==BKSPC) { t_--; rubout(t_); continue; }
   if (!isgraph(c_) && !isspace(c_)) { continue; }
   if (!bins(c_, t_)) return 0; else t_++; } return 1; }
s v delete(tg t_) {
   if(t_.e!=b+bs) t_.e++; i n_=b+bs-t_.e; i tn_=t_.e-t_.st;
   memmove(t_.st, t_.e, n_); g-=tn_; gs+=tn_; }
s i cmdloop(void) {
   i q_; c c_; tg t_; for (q_=0;q_==0;) { draw(); c_=getchar();
   switch (c_) {
      case C_D: doscrl(lines/2); break; case C_U: doscrl(-lines/2); break;
      case 'q': case EOF: q_=1; break;
      case 'i': t_.st=hunt(); if(t_.st==0) break; if (!insertmode(t_.st)) return 0; break;
      case 'a': t_.st=hunt(); if(t_.st==0) break; if(t_.st!=b+bs) t_.st++;
         if (!insertmode(t_.st)) return 0; break;
      case 'w': if(!bsave()) return 0; break;
      case 'd': t_.st=hunt(); t_.e=hunt(); if (t_.st==0 || t_.e==0) break;
         delete(t_); break;
      case 'c': t_.st=hunt(); t_.e=hunt(); if (t_.st==0 || t_.e==0) break;
         delete(t_); insertmode(t_.st); break; } }
   return 1; }
s v ed(void) { if (!bread()) return; scrl=0; if (cmdloop()) cls(); }
s v dtlines(void) {
   struct winsize w_; ioctl(0, TIOCGWINSZ, &w_);
   lines = w_.ws_row; cols = w_.ws_col; }
s v raw(void) { system("stty cbreak -echo"); }
s v unraw(void) { system("stty cooked echo"); }
i main(i argc, c **argv) {
   dtlines();
   if (argc != 2) { err("missing file arg"); return 2; }
   else { raw(); fn=argv[1]; ed(); unraw(); return 0; } }
