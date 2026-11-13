/*
 * rforkfn (lib9.h): rfork(RFPROC|flags) whose child runs f(a), then exits, as fork's child
 * would (fork.c). Here the child is a fork's that calls f; wasm's (../wasm/rforkfn.c) starts
 * the child at f, with nothing of the caller's stack.
 */
#include "lib.h"
#include <errno.h>
#include <unistd.h>
#include "sys9.h"

int
rforkfn(int flags, void (*f)(void*), void *a)
{
	int n;

	n = _RFORK(RFPROC|flags);
	if(n < 0)
		_syserrno();
	if(n == 0){
		_detachbuf();
		_sessleader = 0;
		(*f)(a);
		_exit(0);
	}
	return n;
}
