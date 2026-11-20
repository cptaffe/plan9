/*
 * pANS stdio -- perror
 */
#include "iolib.h"
#include <errno.h>
static void uperror(const char *s){
	extern int errno;
	if(s!=NULL && *s != '\0') fputs(s, stderr), fputs(": ", stderr);
	fputs(strerror(errno), stderr);
	putc('\n', stderr);
	fflush(stderr);
}

void
perror(const char *s)
{
	flockfile(stderr);
	uperror(s);
	funlockfile(stderr);
}
