#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include <complete.h>
#include <acme.h>
#include <adraw.h>
#include "impl.h"

char*
_adrawstrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

char*
_adrawvsmprint(char *fmt, va_list arg)
{
	char *s, *t;

	if((s = vsmprint(fmt, arg)) == nil)
		sysfatal("out of memory");
	t = _adrawstrdup(s);
	free(s);
	return t;
}

char*
_adrawsmprint(char *fmt, ...)
{
	va_list arg;
	char *s;

	va_start(arg, fmt);
	s = _adrawvsmprint(fmt, arg);
	va_end(arg);
	return s;
}

Rune*
_adrawrunesmprint(char *fmt, ...)
{
	va_list arg;
	Rune *r, *t;
	long n;

	va_start(arg, fmt);
	r = runevsmprint(fmt, arg);
	va_end(arg);
	if(r == nil)
		sysfatal("out of memory");
	n = runestrlen(r)+1;
	t = emalloc(n*sizeof(Rune));
	memmove(t, r, n*sizeof(Rune));
	free(r);
	return t;
}

char*
_adrawquote(char *s)
{
	char *q, *t;

	if((q = quotestrdup(s)) == nil)
		sysfatal("out of memory");
	t = _adrawstrdup(q);
	free(q);
	return t;
}
