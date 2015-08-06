/* buffer.h (c) 2015 Tom Wright */

int bufread(char *path);
int bufwrite(char *path);
int bufinsert(char c, char *t);
int bufinsertstr(char *start, char *end, char *t);
void bufdelete(char *start, char *end);
char *getbufstart(void);
char *getbufend(void);

#define inbuf(p) (p >= getbufstart() && p <= getbufend())

char *endofline(char *p);
