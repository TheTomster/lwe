/* (C) 2015 Tom Wright. */

int show_whitespace;

void initcurses(void);
void clrscreen(void);
void present(void);

int scroll_line(void);
void set_scroll(int n);
void adjust_scroll(int delta);

/* Get window boundaries */
char *winstart(void);
char *winend(void);

int winrows(void);
int wincols(void);

int screenlines(char *start);
char *skipscreenlines(char *start, int lines);

void drawmodeline(char *filename, char *mode);
void drawtext(void);
void drawdisamb(char c, int lvl, int toskip);
void drawlinelbls(int lvl, int off);
void drawlineoverlay(void);
void drawmessage(char *msg);
void drawyanks(void);
void draw_eof(void);

int skips(int lvl);
int onlymatch(char c, int lvl, int toskip);

int count(char c);
int countwithin(char *start, char *end, char c);

void refresh_bounds(void);

void movecursor(char *p);
