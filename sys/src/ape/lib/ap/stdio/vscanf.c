/*
 * C99 stdio -- vscanf, vsscanf
 */
#include "iolib.h"

int vscanf(const char *fmt, va_list args){
	return vfscanf(stdin, fmt, args);
}

int vsscanf(const char *s, const char *fmt, va_list args){
	int n;
	FILE *f=_IO_sopenr(s);
	if(f==NULL)
		return EOF;
	n=vfscanf(f, fmt, args);
	_IO_sclose(f);
	return n;
}
