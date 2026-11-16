#include <string.h>
#include <stdlib.h>

char*
strndup(const char *s, size_t n)
{
	char *p, *e;

	if((e = memchr(s, 0, n)) != NULL)
		n = e-s;
	p = malloc(n+1);
	if(p){
		memmove(p, s, n);
		p[n] = 0;
	}
	return p;
}
