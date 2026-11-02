/*
 * APE's system calls: the machines' are made by ../syscall from libc/9syscall,
 * as stubs in assembler.  wasm's stubs are libc/wasm/sys.c, in C, on the
 * runtime's imports; these are the same stubs, under the names ../plan9/sys9.h
 * gives them.  What the kernel sees of an APE program is what it sees of any
 * Plan 9 program.  This file is compiled as Plan 9 C, with <libc.h>.
 */
#define	await		_AWAIT
#define	bind		_BIND
#define	chdir		_CHDIR
#define	close		_CLOSE
#define	create		_CREATE
#define	dup		_DUP
#define	errstr		_ERRSTR
#define	exec		_EXEC
#define	_exits		_EXITS
#define	fauth		_FAUTH
#define	fd2path		_FD2PATH
#define	fstat		_FSTAT
#define	fversion	_FVERSION
#define	fwstat		_FWSTAT
#define	mount		_MOUNT
#define	noted		_NOTED
#define	open		_OPEN
#define	pipe		_PIPE
#define	pread		_PREAD
#define	pwrite		_PWRITE
#define	remove		_REMOVE
#define	rendezvous	_RENDEZVOUS
#define	rfork		_RFORK
#define	seek		_SEEK
#define	sleep		_SLEEP
#define	stat		_STAT
#define	unmount		_UNMOUNT
#define	wstat		_WSTAT
#define	alarm		_ALARM
#define	notify		notify9		/* _NOTIFY, below: the handler's type is not libc's */
#define	werrstr		seterr		/* not in APE */

#include "../../../../libc/wasm/sys.c"

static Tos tos;
Tos *_tos = &tos;

void
seterr(char *f, ...)
{
	char b[ERRMAX];

	strncpy(b, f, sizeof b);
	b[sizeof b-1] = 0;
	_ERRSTR(b, sizeof b);
}

/*
 * ../plan9/signal.c's handler returns int, libc's void, and on wasm a call of a
 * function by a pointer of another type is a trap.
 */
static int (*handler)(void*, char*);

static void
callhandler(void *u, char *s)
{
	(*handler)(u, s);
}

int
_NOTIFY(int (*f)(void*, char*))
{
	handler = f;
	return notify9(f != nil ? callhandler : nil);
}

/* A process alone in its memory has no one to wait for: as libc/wasm/main9.c. */
int
_SEMACQUIRE(long *p, int block)
{
	USED(block);
	if(*p > 0){
		--*p;
		return 1;
	}
	return 0;
}

int
_TSEMACQUIRE(long *p, ulong ms)
{
	USED(ms);
	return _SEMACQUIRE(p, 0);
}

long
_SEMRELEASE(long *p, long n)
{
	*p += n;
	return *p;
}

/* The kernel has no segments: a wasm module has one memory (select's ../plan9/_buf.c wants them). */
static int
noseg(void)
{
	seterr("no segments on wasm");
	return -1;
}

int _SEGATTACH(int a, char *c, void *v, ulong n) { USED(a, c, v, n); return noseg(); }
int _SEGBRK(void *a, void *b) { USED(a, b); return noseg(); }
int _SEGDETACH(void *a) { USED(a); return noseg(); }
int _SEGFLUSH(void *a, ulong n) { USED(a, n); return noseg(); }
int _SEGFREE(void *a, ulong n) { USED(a, n); return noseg(); }
