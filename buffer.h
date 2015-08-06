/* buffer.h (c) 2015 Tom Wright */

int bufread(char *path);
int bufwrite(char *path);
char *bufinsert(char c, char *t);
char *bufinsertstr(char *start, char *end, char *t);
void bufdelete(char *start, char *end);
char *getbufstart(void);
char *getbufend(void);

#define inbuf(p) (p >= getbufstart() && p <= getbufend())

char *endofline(char *p);
