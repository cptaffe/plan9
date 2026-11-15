#include <u.h>
#include <libc.h>
#include <tos.h>
#include "wasm.h"

P9(rfork) extern int p9web·rfork(int);

char _procforkmark[1];

#ifdef __clang__
__attribute__((used))
#endif
EXPORT(p9web_fork) void
p9web_fork(void)
{
}

int
procfork(int flags)
{
	int r;

	r = p9web·rfork(flags);
	if(r == 0)
		_tos->pid = getpid();
	return r < 0 ? -1 : r;
}
