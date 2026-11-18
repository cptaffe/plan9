#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern void _doatexits(void);

void
exit(int status)
{
	_doatexits();
	fflush(NULL);
	_exit(status);
}
