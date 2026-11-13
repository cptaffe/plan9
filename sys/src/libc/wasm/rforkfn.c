/*
 * rforkfn is rfork with RFPROC whose child runs f(a), then exits. The child is a copy of this
 * memory that starts at forkentry, not at the call, so no stack is copied: a program that forks
 * only so needs no asyncify. Only a program that calls it has forkentry and its imports.
 */
#include <u.h>
#include <libc.h>
#include <tos.h>

__attribute__((import_module("p9web"), import_name("forkentry"))) extern int p9forkentry(int);

static void	(*entryf)(void*);
static void	*entrya;

int
rforkfn(int flags, void (*f)(void*), void *a)
{
	int r;

	entryf = f;
	entrya = a;
	r = p9forkentry(flags|RFPROC);
	return r < 0 ? -1 : r;
}

__attribute__((export_name("p9web_forkentry"))) void
forkentry(void)
{
	_tos->pid = getpid();
	(*entryf)(entrya);
	_exits(nil);
}
