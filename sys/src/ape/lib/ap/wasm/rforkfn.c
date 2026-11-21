/*
 * rforkfn (lib9.h): rfork(RFPROC|flags) whose child runs f(a), then exits, as fork's child
 * would (fork.c). The child is a copy of this memory that starts at start, not at the call
 * (_RFORKFN, libc/wasm/sys.c): a program whose children only do so, as a shell's or a
 * subprocess's do, has no fork, and needs no asyncify.
 */
#include "../plan9/lib.h"
#include <errno.h>
#include <unistd.h>
#include "../plan9/sys9.h"

typedef struct Start Start;
struct Start {
	void	(*f)(void*);
	void	*a;
};

static void
start(void *v)
{
	Start *s;

	s = v;
	_detachbuf();
	_grouppid = 0;
	_sessleader = 0;
	_itimerms = 0;
	(*s->f)(s->a);
	_exit(0);
}

int
rforkfn(int flags, void (*f)(void*), void *a)
{
	Start s;
	int n;

	s.f = f;
	s.a = a;
	n = _RFORKFN(flags, start, &s);
	if(n < 0)
		_syserrno();
	return n;
}
