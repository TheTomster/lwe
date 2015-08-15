#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "bang.h"
#include "err.h"

enum pipe_err { PIPE_OK, PIPE_ERR };

enum write_err { WRITE_OK, WRITE_ERR };

#define NULL_OUTPUT ((struct bang_output) { .buf = NULL, .sz = 0 })

static enum pipe_err openpipes(int in[2], int out[2], int err[2]);
static void closepipes(int in[2], int out[2], int err[2]);
static void child_exec(int in[2], int out[2], int err[2], char *cmd);
static enum write_err write_input(int in, char *data, int sz);
static struct bang_output collect_output(int out);

static enum pipe_err openpipes(int in[2], int out[2], int err[2])
{
	if (pipe(in) == -1) {
		return PIPE_ERR;
	}
	if (pipe(out) == -1) {
		close(in[0]);
		close(in[1]);
		return PIPE_ERR;
	}
	if (pipe(err) == -1) {
		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		return PIPE_ERR;
	}
	return PIPE_OK;
}

static void closepipes(int in[2], int out[2], int err[2])
{
	for (int i = 0; i < 2; i++) {
		close(in[i]);
		close(out[i]);
		close(err[i]);
	}
}

static void child_exec(int in[2], int out[2], int err[2], char *cmd)
{
	close(in[1]);
	close(out[0]);
	close(err[0]);
	dup2(in[0], STDIN_FILENO);
	dup2(out[1], STDOUT_FILENO);
	dup2(err[1], STDERR_FILENO);
	execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
}

static enum write_err write_input(int in, char *data, int sz)
{
	ssize_t total_written = 0;
	while (total_written < sz) {
		char *write_ptr = data + total_written;
		int left = sz - total_written;
		ssize_t written = write(in, write_ptr, left);
		if (written == -1)
			return WRITE_ERR;
		total_written += written;
	}
	close(in);
	return WRITE_OK;
}

static struct bang_output collect_output(int fd)
{
	char *allocated = malloc(8192);
	if (allocated == NULL) {
		close(fd);
		return NULL_OUTPUT;
	}
	int allocsz = 8192;
	int datasz = 0;
	while (1) {
		ssize_t r = read(fd, allocated + datasz, allocsz - datasz);
		if (r == 0)
			break;
		datasz += r;
		if (allocsz - datasz == 0) {
			char *reallocated = realloc(allocated, allocsz * 2);
			if (reallocated == NULL) {
				free(allocated);
				close(fd);
				return NULL_OUTPUT;
			}
			allocated = reallocated;
			allocsz *= 2;
		}
	}
	close(fd);
	return (struct bang_output) { .buf = allocated, .sz = datasz };
}

int bang(
	struct bang_output *out, struct bang_output *err,
	char *cmd, char *input, int input_sz)
{
	int inpipe[2];
	int outpipe[2];
	int errpipe[2];
	if (openpipes(inpipe, outpipe, errpipe) != PIPE_OK) {
		seterr("pipe");
		*out = NULL_OUTPUT;
		*err = NULL_OUTPUT;
		return -1;
	}

	int child = fork();
	if (child == -1) {
		seterr("fork");
		closepipes(inpipe, outpipe, errpipe);
		*out = NULL_OUTPUT;
		*err = NULL_OUTPUT;
		return -1;
	}

	if (child == 0) {
		child_exec(inpipe, outpipe, errpipe, cmd);
	} else {
		close(inpipe[0]);
		close(outpipe[1]);
		close(errpipe[1]);
		write_input(inpipe[1], input, input_sz);
		*out = collect_output(outpipe[0]);
		*err = collect_output(errpipe[0]);
		int status;
		if (wait(&status) < 0)
			return -1;
		if (WEXITSTATUS(status) != EXIT_SUCCESS)
			return -1;
	}

	return 0;
}
