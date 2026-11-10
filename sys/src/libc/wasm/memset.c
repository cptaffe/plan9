#include	<u.h>
#include	<libc.h>

/* wasm's memory.fill (bulk memory) */
void*
memset(void *ap, int c, ulong n)
{
	return __builtin_memset(ap, c, n);
}
