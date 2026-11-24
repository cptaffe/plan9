#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

/* The list ends at a null argument, and then comes the environment: see execl.c. */
int
execle(const char *name, const char *arg0, const char *arg1, ...)
{
	va_list ap, b;
	const char **v, **e;
	int n, r;

	n = 0;
	if(arg0 != 0 && arg1 != 0){
		va_start(ap, arg1);
		va_copy(b, ap);
		for(n = 1; va_arg(b, char*) != 0; n++)
			;
		va_end(b);
	}else
		va_start(ap, arg1);
	v = malloc((n+3)*sizeof(char*));
	if(v == 0){
		va_end(ap);
		return -1;
	}
	v[0] = arg0;
	v[1] = arg1;
	if(arg0 == 0)
		e = (const char**)arg1;
	else if(arg1 == 0)
		e = va_arg(ap, const char**);
	else{
		for(n = 2; (v[n] = va_arg(ap, char*)) != 0; n++)
			;
		e = va_arg(ap, const char**);
	}
	va_end(ap);
	r = execve(name, (char**)v, (char**)e);
	free(v);
	return r;
}
