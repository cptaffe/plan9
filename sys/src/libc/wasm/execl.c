#include <u.h>
#include <libc.h>

int
execl(char *f, ...)
{
	va_list a, b;
	char **v;
	int n;

	va_start(a, f);
	va_copy(b, a);
	for(n = 0; va_arg(a, char*) != nil; n++)
		;
	va_end(a);
	v = malloc((n+1)*sizeof(char*));
	if(v == nil)
		return -1;
	for(n = 0; (v[n] = va_arg(b, char*)) != nil; n++)
		;
	va_end(b);
	exec(f, v);
	free(v);
	return -1;
}
