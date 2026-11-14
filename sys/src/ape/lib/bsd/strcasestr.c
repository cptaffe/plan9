#include <string.h>
#include <strings.h>

char *
strcasestr(const char *h, const char *n)
{
	size_t l;

	l = strlen(n);
	for(; *h; h++)
		if(strncasecmp(h, n, l) == 0)
			return (char*)h;
	return l == 0 ? (char*)h : NULL;
}
