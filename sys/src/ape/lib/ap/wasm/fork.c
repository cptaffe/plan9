/*
 * ../plan9/fork.c for wasm: by _PROCFORK, not _RFORK, so that it, and only it, loads the fork
 * that asyncify makes (procfork9.c). A program that starts processes only with posix_spawn or
 * system does not fork.
 */
#include "../plan9/lib.h"
#include <errno.h>
#include <unistd.h>
#include "../plan9/sys9.h"

pid_t
fork(void)
{
	int n;

	n = _PROCFORK(RFENVG|RFFDG|RFPROC);
	if(n < 0)
		_syserrno();
	if(n == 0) {
		_detachbuf();
		_grouppid = 0;
		_sessleader = 0;
		_itimerms = 0;
	}
	return n;
}
