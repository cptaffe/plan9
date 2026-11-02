#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

extern char **_arglist(const char*, va_list);

/* ../plan9/execlp.c's, by way of execvp, which has the same BUG. */
int
execlp(const char *name, const char *arg0, ...)
{
	va_list ap;
	char **v;
	int r;

	va_start(ap, arg0);
	v = _arglist(arg0, ap);
	va_end(ap);
	if(v == 0)
		return -1;
	r = execvp(name, (const char**)v);
	free(v);
	return r;
}
