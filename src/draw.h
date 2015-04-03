/* (C) 2015 Tom Wright. */

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
