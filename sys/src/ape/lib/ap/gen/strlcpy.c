#include <string.h>

size_t
strlcpy(char *d, const char *s, size_t n)
{
	size_t l;

	l = strlen(s);
	if(n > 0){
		if(l < n)
			memcpy(d, s, l+1);
		else{
			memcpy(d, s, n-1);
			d[n-1] = 0;
		}
	}
	return l;
}

size_t
strlcat(char *d, const char *s, size_t n)
{
	size_t l;
	char *e;

	if((e = memchr(d, 0, n)) == NULL)
		return n + strlen(s);
	l = e - d;
	return l + strlcpy(d+l, s, n-l);
}
