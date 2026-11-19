#include <u.h>
#include <libc.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

char*
_winevsmprint(char *s, va_list v)
{
	s = vsmprint(s, v);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}
