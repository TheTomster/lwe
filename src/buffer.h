/* buffer.h (c) 2015 Tom Wright */

bool bufread(char *path);
bool bufwrite(char *path);
bool bufinsert(char c, char *t);
void bufdelete(char *start, char *end);
char *getbufptr(void);
char *getbufend(void);

