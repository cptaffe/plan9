#include <u.h>
#include <libc.h>
#include <tos.h>
#include "wasm.h"

enum { NPRIVATES = 16 };

extern void main(int, char**);
extern char __stack_high[];

Tos *_tos;
void *_privates;
int _nprivates;
char *argv0;
static Tos tos;
static void *privates[NPRIVATES];
#ifdef __clang__
extern char __heap_base[];
static char *bloc = __heap_base;
#endif
static char asyncbuf[65536];

void
_start(void)
{
	int *a;

	_tos = &tos;
	_privates = privates;
	_nprivates = NPRIVATES;
	tos.pid = getpid();
	a = *(int**)(__stack_high - sizeof(int*));
	main(a[0], (char**)(a+1));
	exits("main");
}

EXPORT(p9web_plan9) void
p9web_plan9(void)
{
}

EXPORT(p9web_async_buf) char*
p9web_async_buf(void)
{
	return (char*)(((uintptr)asyncbuf + 15) & ~15);
}

EXPORT(p9web_async_len) long
p9web_async_len(void)
{
	return sizeof asyncbuf - 16;
}

int
brk_(void *p)
{
	ulong have, want;

	have = wasm·memory_size();
	want = ((uvlong)(uintptr)p + 65535) >> 16;
	if(want > have && wasm·memory_grow(want-have) == (ulong)-1){
		werrstr("no memory");
		return -1;
	}
	return 0;
}

#ifdef __clang__
int
brk(void *p)
{
	if(brk_(p) < 0)
		return -1;
	bloc = p;
	return 0;
}

void*
sbrk(ulong n)
{
	uintptr b;

	b = ((uintptr)bloc + 7) & ~7;
	if(b+n < b){
		werrstr("no memory");
		return (void*)-1;
	}
	if(brk_((void*)(b+n)) < 0)
		return (void*)-1;
	bloc = (char*)b + n;
	return (void*)b;
}
#endif

int
_tas(int *p)
{
	int v;

	v = *p;
	*p = 1;
	return v;
}

ulong getfcr(void) { return 0; }
void setfcr(ulong v) { USED(v); }
ulong getfsr(void) { return 0; }
void setfsr(ulong v) { USED(v); }
void cycles(uvlong *t) { *t = 0; }

long ainc(long *p) { return ++*p; }
long adec(long *p) { return --*p; }

#pragma weak semacquire
int semacquire(long *p, int block) { USED(block); if(*p > 0){ --*p; return 1; } return 0; }
#pragma weak tsemacquire
int tsemacquire(long *p, ulong ms) { USED(ms); return semacquire(p, 0); }
#pragma weak semrelease
long semrelease(long *p, long n) { *p += n; return *p; }
