/*
 * pANS stdio -- puts
 */
#include "iolib.h"
static int uputs(const char *s){
	fputs(s, stdout);
	putchar('\n');
	return ferror(stdin)?EOF:0;
}

int
puts(const char *s)
{
	int r;

	flockfile(stdout);
	r = uputs(s);
	funlockfile(stdout);
	return r;
}
