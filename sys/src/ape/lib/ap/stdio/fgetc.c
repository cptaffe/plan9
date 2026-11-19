/*
 * pANS stdio -- fgetc
 */
#include "iolib.h"
static int ufgetc(FILE *f){
	return getc(f);
}

int
fgetc(FILE *f)
{
	int r;

	flockfile(f);
	r = ufgetc(f);
	funlockfile(f);
	return r;
}
