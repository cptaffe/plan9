#include <u.h>
#include <libc.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

char*
_winevsmprint(char *s, va_list v)
{
	char *t;

	if((s = vsmprint(s, v)) == nil)
		sysfatal("out of memory");
	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	free(s);
	return t;
}
