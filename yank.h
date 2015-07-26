/* (C) 2015 Tom Wright. */

int saveyanks(void);
int loadyanks(void);
int yank_sz(void);
void yank_item(char **item, unsigned *len, int n);
void yank_store(char *start, char *end);
