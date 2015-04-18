/* (C) 2015 Tom Wright. */

void initcurses(void);

int scroll_line(void);
void set_scroll(int n);

/* Get window boundaries */
char *winstart(void);
char *winend(void);

int winrows(void);
int wincols(void);

void draw(char *start, char *end);

int screenlines(char *start);
char *skipscreenlines(char *start, int lines);

void pc(char c); // TODO hide this
void drawmodeline(char *filename, char *mode); // TODO hide this
void old_draw(char *filename, char *mode);
void drawdisamb(char c, int lvl, int toskip, char *filename, char *mode);
void drawlinelbls(int lvl, int off, char *filename, char *mode);

int skips(int lvl);
bool onlymatch(char c, int lvl, int toskip);

int count(char c);
int countwithin(char *start, char *end, char c);
