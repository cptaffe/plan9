/*
 * pANS stdio -- feof
 */
#include "iolib.h"
static int ufeof(FILE *f){
	return f->state==END;
}

int
feof(FILE *f)
{
	int r;

	flockfile(f);
	r = ufeof(f);
	funlockfile(f);
	return r;
}
