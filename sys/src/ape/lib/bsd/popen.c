#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "lib.h"
#include "sys9.h"

#define MAXFORKS	20
#define NSYSFILE	3
#define	tst(a,b)	(*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

struct a_fork {
	short	done;
	short	fd;
	int	pid;
	int	status;
};
static struct a_fork the_fork[MAXFORKS];

/* the shell's process: rforkfn's, so that, on wasm, a program that pipes needs no fork */
struct a_child {
	int	myside;
	int	hisside;
	int	to;
	const char	*cmd;
};

static void
child(void *v)
{
	struct a_child *c;
	int i;

	c = v;
	/* myside and hisside reverse roles in child */
	close(c->myside);
	dup2(c->hisside, c->to);
	for (i=NSYSFILE; i<FOPEN_MAX; i++)
		close(i);
	execl("/bin/ape/sh", "sh", "-c", c->cmd, NULL);
	_exit(1);
}

FILE *
popen(const char *cmd, const char *mode)
{
	int p[2];
	int myside, hisside, pid;
	int ind;
	struct a_child c;

	for (ind = 0; ind < MAXFORKS; ind++)
		if (the_fork[ind].pid == 0)
			break;
	if (ind == MAXFORKS)
		return NULL;
	if(pipe(p) < 0)
		return NULL;
	myside = tst(p[WTR], p[RDR]);
	hisside = tst(p[RDR], p[WTR]);
	c.myside = myside;
	c.hisside = hisside;
	c.to = tst(0, 1);
	c.cmd = cmd;
	switch (pid = rforkfn(RFENVG|RFFDG, child, &c)) {
	case -1:
		return NULL;
	default:
		the_fork[ind].pid = pid;
		the_fork[ind].fd = myside;
		the_fork[ind].done = 0;
		close(hisside);
		return(fdopen(myside, mode));
	}
}

int
pclose(FILE *ptr)
{
	int f, r, ind;
	int status;

	f = fileno(ptr);
	fclose(ptr);
	for (ind = 0; ind < MAXFORKS; ind++)
		if (the_fork[ind].fd == f && the_fork[ind].pid != 0)
			break;
	if (ind == MAXFORKS)
		return 0;
	if (!the_fork[ind].done) {
		do {
			while((r = wait(&status)) == -1 && errno == EINTR)
				;
			for (f = 0; f < MAXFORKS; f++)
				if (the_fork[f].pid == r) {
					the_fork[f].done = 1;
					the_fork[f].status = status;
					break;
				}
		} while(r != the_fork[ind].pid && r != -1);
		the_fork[ind].status = r == -1 ? -1 : status;
	}
	the_fork[ind].pid = 0;
	return (the_fork[ind].status);
}
