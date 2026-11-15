#include	<u.h>
#include	<libc.h>
#include	"wasm.h"

void*
memmove(void *a1, void *a2, ulong n)
{
	if((long)n < 0)
		abort();
	wasm·memory_copy(a1, a2, n);
	return a1;
}

void*
memcpy(void *a1, void *a2, ulong n)
{
	return memmove(a1, a2, n);
}
