#include <string.h>

/*
 * wasm's memory.copy (bulk memory), which is memmove's: the
 * copy is right when the two overlap
 */
void*
memmove(void *a1, const void *a2, size_t n)
{
	extern void abort(void);

	if((long)n < 0)
		abort();
	return __builtin_memmove(a1, a2, n);
}

void*
memcpy(void *a1, const void *a2, size_t n)
{
	return memmove(a1, a2, n);
}
