/*
 * pANS stdio -- fputc
 */
#include "iolib.h"
static int ufputc(int c, FILE *f){
	return putc(c, f);	/* This can be made more fair to _IOLBF-mode streams */
}

int
fputc(int c, FILE *f)
{
	int r;

	flockfile(f);
	r = ufputc(c, f);
	funlockfile(f);
	return r;
}
