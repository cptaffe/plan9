#include "../plan9/lib.h"
#include <errno.h>
#include "../plan9/sys9.h"

/* The heap is the module's memory: the runtime has no brk to call. */
extern char __heap_base[];
static char *bloc = __heap_base;

static int
grow(unsigned long n)
{
	unsigned long have, want;

	have = __builtin_wasm_memory_size(0);
	want = ((unsigned long long)n + 65535) >> 16;
	if(want > have && __builtin_wasm_memory_grow(0, want-have) == (unsigned long)-1){
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

char *
brk(char *p)
{
	unsigned long n;

	n = (unsigned long)p;
	n += 7;
	n &= ~7;
	if(grow(n) < 0)
		return (char *)-1;
	bloc = (char *)n;
	return 0;
}

void *
sbrk(unsigned long n)
{
	char *b;

	b = (char*)(((unsigned long)bloc + 7) & ~7UL);
	if(n > ~0UL - 7 || (unsigned long)b + ((n + 7) & ~7UL) < (unsigned long)b){
		errno = ENOMEM;
		return (void *)-1;
	}
	n += 7;
	n &= ~7;
	if(grow((unsigned long)b + n) < 0)
		return (void *)-1;
	bloc = b + n;
	return b;
}
