/*
 * rfork's RFPROC (sys.c): the runtime unwinds the stack with asyncify, the kernel copies the
 * process, and both rewind into this call (D93). Only a program that loads this has the fork
 * import, and so needs asyncify.
 */
#include <u.h>
#include <libc.h>
#include <tos.h>

__attribute__((import_module("p9web"), import_name("fork"))) extern int p9fork(int);

int
procfork(int flags)
{
	int r;

	r = p9fork(flags);
	if(r == 0)
		_tos->pid = getpid();
	return r < 0 ? -1 : r;
}
