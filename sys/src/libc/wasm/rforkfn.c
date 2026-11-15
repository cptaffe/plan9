#include <u.h>
#include <libc.h>
#include <tos.h>
#include "wasm.h"

P9(forkentry) extern int p9web·forkentry(int);

static void	(*entryf)(void*);
static void	*entrya;

int
rforkfn(int flags, void (*f)(void*), void *a)
{
	int r;

	entryf = f;
	entrya = a;
	r = p9web·forkentry(flags|RFPROC);
	return r < 0 ? -1 : r;
}

EXPORT(p9web_forkentry) void
p9web_forkentry(void)
{
	_tos->pid = getpid();
	(*entryf)(entrya);
	_exits(nil);
}
