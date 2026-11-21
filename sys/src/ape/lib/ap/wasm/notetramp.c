#include "../plan9/lib.h"
#include "../plan9/sys9.h"
#include <signal.h>
#include <setjmp.h>

/*
 * A note comes at the end of a system call, on top of the program's stack (libc/wasm/sys.c),
 * so there is no pc to change and restore: the handler runs where it is, and NCONT goes back
 * to the call.  A handler has the type POSIX gives it: on wasm a call by a pointer of another
 * type is a trap, and the machines' trailing char* and Ureg* could not be passed.
 */
void
_notetramp(int sig, void (*hdlr)(int, char*, Ureg*), Ureg *u)
{
	void (*f)(int);

	USED(u);
	f = (void(*)(int))hdlr;
	(*f)(sig);
	_NOTED(0);	/* NCONT */
}

void
siglongjmp(sigjmp_buf j, int ret)
{
	if(j[0])
		_psigblocked = j[1];
	longjmp(j+2, ret);
}
