/*
 * pANS stdio -- vsnprintf
 *	C99: at most nbuf-1 characters and a NUL; the number there
 *	would have been
 */
#include "iolib.h"
#include <string.h>

int vsnprintf(char *buf, size_t nbuf, const char *fmt, va_list args){
	int n, m;
	char *s;
	FILE *f=_IO_sopenw();
	if(f==NULL)
		return -1;
	n=vfprintf(f, fmt, args);
	s=_IO_sclose(f);
	if(s==NULL)
		return -1;
	if(nbuf>0){
		m=n<0? 0: n<nbuf? n: nbuf-1;
		memmove(buf, s, m);
		buf[m]='\0';
	}
	free(s);
	return n;
}
