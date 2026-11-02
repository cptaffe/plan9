#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

/* ../plan9/execl.c takes the address of an argument: wasm's are not on a stack in a row. */
extern char **environ;

char**
_arglist(const char *arg0, va_list ap)
{
	va_list b;
	const char **v;
	int n;

	va_copy(b, ap);
	for(n = 0; arg0 != 0 && va_arg(b, char*) != 0; n++)
		;
	va_end(b);
	v = malloc((n+2)*sizeof(char*));
	if(v == 0)
		return 0;
	v[0] = arg0;
	if(arg0 != 0)
		for(n = 1; (v[n] = va_arg(ap, char*)) != 0; n++)
			;
	return (char**)v;
}

int
execl(const char *name, const char *arg0, ...)
{
	va_list ap;
	char **v;
	int r;

	va_start(ap, arg0);
	v = _arglist(arg0, ap);
	va_end(ap);
	if(v == 0)
		return -1;
	r = execve(name, (const char**)v, (const char**)environ);
	free(v);
	return r;
}
