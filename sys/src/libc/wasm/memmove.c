#include	<u.h>
#include	<libc.h>

/*
 * wasm's memory.copy (bulk memory), which is memmove's: the
 * copy is right when the two overlap
 */
void*
memmove(void *a1, void *a2, ulong n)
{
	if((long)n < 0)
		abort();
	return __builtin_memmove(a1, a2, n);
}

void*
memcpy(void *a1, void *a2, ulong n)
{
	return memmove(a1, a2, n);
}
