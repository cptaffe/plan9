#include <stdlib.h>
#include <unistd.h>
/*
 * C11: quick_exit is exit with its own functions, and no flush of streams
 */
#define	NONEXIT	32
static void (*fns[NONEXIT])(void);
static int nfns;

int
at_quick_exit(void (*f)(void))
{
	if(nfns >= NONEXIT)
		return 1;
	fns[nfns++] = f;
	return 0;
}

void
quick_exit(int status)
{
	while(nfns > 0)
		(*fns[--nfns])();
	_exit(status);
}
