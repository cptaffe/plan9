#include "../stdio/iolib.h"

int (*_lockfile)(FILE *, int);
void (*_unlockfile)(FILE *);

void
flockfile(FILE *f)
{
	if(_lockfile != NULL)
		_lockfile(f, 0);
}

int
ftrylockfile(FILE *f)
{
	if(_lockfile != NULL)
		return _lockfile(f, 1);
	return 0;
}

void
funlockfile(FILE *f)
{
	if(_unlockfile != NULL)
		_unlockfile(f);
}
