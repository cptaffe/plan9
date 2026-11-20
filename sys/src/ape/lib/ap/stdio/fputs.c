/*
 * pANS stdio -- fputs
 */
#include "iolib.h"
static int ufputs(const char *s, FILE *f){
	while(*s) putc(*s++, f);
	return ferror(f)?EOF:0;
}

int
fputs(const char *s, FILE *f)
{
	int r;

	flockfile(f);
	r = ufputs(s, f);
	funlockfile(f);
	return r;
}
