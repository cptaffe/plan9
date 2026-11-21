/*
 * pANS stdio -- ferror
 */
#include "iolib.h"
static int uferror(FILE *f){
	return f->state==ERR;
}

int
ferror(FILE *f)
{
	int r;

	flockfile(f);
	r = uferror(f);
	funlockfile(f);
	return r;
}
