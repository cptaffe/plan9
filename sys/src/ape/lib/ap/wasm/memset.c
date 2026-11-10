#include <string.h>

/* wasm's memory.fill (bulk memory) */
void*
memset(void *ap, int c, size_t n)
{
	return __builtin_memset(ap, c, n);
}
