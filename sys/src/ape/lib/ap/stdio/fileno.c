/*
 * Posix stdio -- fileno
 */
#include "iolib.h"
static int ufileno(FILE *f){
	if(f==NULL)
		return -1;
	else
		return f->fd;
}

int
fileno(FILE *f)
{
	int r;

	flockfile(f);
	r = ufileno(f);
	funlockfile(f);
	return r;
}
