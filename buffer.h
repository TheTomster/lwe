/* buffer.h (c) 2015 Tom Wright */

bool bufread(char *path);
bool bufwrite(char *path);
bool bufinsert(char c, char *t);
bool bufinsertstr(char *start, char *end, char *t);
void bufdelete(char *start, char *end);
char *getbufstart(void);
char *getbufend(void);

#define inbuf(p) (p >= getbufstart() && p <= getbufend())

char *startofline(int off);
char *endofline(char *p);
