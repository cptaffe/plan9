#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern void _doatexits(void);
extern void _IO_flushall(void);

void
exit(int status)
{
	_doatexits();
	_IO_flushall();
	_exit(status);
}
