#include	<u.h>
#include	<libc.h>
#include	"wasm.h"

void*
memset(void *ap, int c, ulong n)
{
	wasm·memory_fill(ap, c, n);
	return ap;
}
