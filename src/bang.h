/* (C) 2015 Tom Wright */

struct bang_output {
	char *buf;
	int sz;
};

struct bang_output bang(char *cmd, char *input, int input_sz);
