/*
 * What each architecture's main9.s, argv0.s, tas.s, getcallerpc.s and sbrk do, for wasm: _start
 * gets the arguments from the runtime and runs main, and the heap is the module's memory.
 */
#include <u.h>
#include <libc.h>
#include <tos.h>

enum { NPRIVATES = 16 };

__attribute__((import_module("wasi_snapshot_preview1"), import_name("args_sizes_get"))) extern int argsizes(int*, int*);
__attribute__((import_module("wasi_snapshot_preview1"), import_name("args_get"))) extern int argsget(char**, char*);
extern void main(int, char**);
extern char __heap_base[];

Tos *_tos;
void *_privates;
int _nprivates;
char *argv0;
static Tos tos;
static void *privates[NPRIVATES];
static char *bloc = __heap_base;

void
_start(void)
{
	static int argc;
	static char **argv;
	static char *buf;
	int n;

	_tos = &tos;
	_privates = privates;
	_nprivates = NPRIVATES;
	tos.pid = getpid();
	argsizes(&argc, &n);
	argv = malloc((argc+1)*sizeof(char*));
	buf = malloc(n);
	argsget(argv, buf);
	argv[argc] = nil;
	main(argc, argv);
	exits("main");
}

/* This is a Plan 9 program, not a WASI one: its stats are 9P2000's. */
__attribute__((export_name("p9web_plan9"))) void
isplan9(void)
{
}

__attribute__((export_name("p9web_async_buf"))) char*
asyncbuf(void)
{
	static char buf[65536] __attribute__((aligned(16)));
	return buf;
}

int
brk(void *p)
{
	ulong have, want;

	have = __builtin_wasm_memory_size(0);
	want = ((uintptr)p + 65535) >> 16;
	if(want > have && __builtin_wasm_memory_grow(0, want-have) == (ulong)-1){
		werrstr("no memory");
		return -1;
	}
	bloc = p;
	return 0;
}

void*
sbrk(ulong n)
{
	uintptr b;

	b = ((uintptr)bloc + 7) & ~7;
	if(brk((void*)(b+n)) < 0)
		return (void*)-1;
	return (void*)b;
}

/* One fiber runs at a time, and a switch is at a call: no atomic instruction is needed. */
int
_tas(int *p)
{
	int v;

	v = *p;
	*p = 1;
	return v;
}

uintptr getcallerpc(void *p) { USED(p); return 0; }
ulong getfcr(void) { return 0; }
void setfcr(ulong v) { USED(v); }
ulong getfsr(void) { return 0; }
void setfsr(ulong v) { USED(v); }
void cycles(uvlong *t) { *t = 0; }

long ainc(long *p) { return ++*p; }
long adec(long *p) { return --*p; }

/* A process alone in its memory has no one to wait for; libthread has its own (libthread/wasm.c). */
#pragma weak semacquire
int semacquire(long *p, int block) { USED(block); if(*p > 0){ --*p; return 1; } return 0; }
#pragma weak tsemacquire
int tsemacquire(long *p, ulong ms) { USED(ms); return semacquire(p, 0); }
#pragma weak semrelease
long semrelease(long *p, long n) { *p += n; return *p; }

/* port/execl.c takes the address of its first argument: wasm's are not on a stack in a row. */
int
execl(char *f, ...)
{
	va_list a, b;
	char **v;
	int n;

	va_start(a, f);
	va_copy(b, a);
	for(n = 0; va_arg(a, char*) != nil; n++)
		;
	va_end(a);
	v = malloc((n+1)*sizeof(char*));
	if(v == nil)
		return -1;
	for(n = 0; (v[n] = va_arg(b, char*)) != nil; n++)
		;
	va_end(b);
	exec(f, v);
	free(v);
	return -1;
}
