/* (C) 2015 Tom Wright */

struct bang_output {
	char *buf;
	int sz;
};

int bang(
	struct bang_output *out, struct bang_output *err,
	char *cmd, char *input, int input_sz);
