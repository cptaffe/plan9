#include <u.h>
#include <libc.h>
#include <tos.h>
#include "wasm.h"

P9(forkentry) extern int p9web·forkentry(int, void(*)(void*), void*);

int
rforkfn(int flags, void (*f)(void*), void *a)
{
	int r;

	r = p9web·forkentry(flags|RFPROC, f, a);
	return r < 0 ? -1 : r;
}

EXPORT(p9web_forkentry) void
p9web_forkentry(void (*f)(void*), void *a)
{
	_tos->pid = getpid();
	(*f)(a);
	_exits(nil);
}
