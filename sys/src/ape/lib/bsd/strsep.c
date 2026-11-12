#include <string.h>

/* BSD's strsep: the token at *sp, ended by a byte of delim, and *sp past it (NULL at the end) */
char*
strsep(char **sp, const char *delim)
{
	char *s, *e;

	if((s = *sp) == NULL)
		return NULL;
	e = strpbrk(s, delim);
	if(e != NULL)
		*e++ = '\0';
	*sp = e;
	return s;
}
