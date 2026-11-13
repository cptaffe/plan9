/*
 * rfork's RFPROC (sys.c): the kernel unwinds the stack with asyncify, copies the process, and
 * both rewind into this call (D93). Only a program that loads this forks so, and its mark,
 * p9web_fork, has the build run it through asyncify (scripts/forks.mjs).
 */
#include <u.h>
#include <libc.h>
#include <tos.h>

__attribute__((import_module("p9web"), import_name("rfork"))) extern int p9rfork(int);

__attribute__((export_name("p9web_fork"), used)) void
_p9webfork(void)
{
}

int
procfork(int flags)
{
	int r;

	r = p9rfork(flags);
	if(r == 0)
		_tos->pid = getpid();
	return r < 0 ? -1 : r;
}
