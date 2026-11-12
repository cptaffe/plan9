/*
 * POSIX 2008 getdelim and getline: a line, to its delimiter, in a buffer that grows
 */
#include "iolib.h"
#include <errno.h>

ssize_t
getdelim(char **lp, size_t *np, int delim, FILE *f)
{
	char *s;
	size_t n, m;
	int c;

	if(lp == NULL || np == NULL){
		errno = EINVAL;
		return -1;
	}
	if(*lp == NULL)
		*np = 0;
	n = 0;
	while((c = getc(f)) != EOF){
		if(n+2 > *np){
			m = *np < 64 ? 128 : *np*2;
			if((s = realloc(*lp, m)) == NULL)
				return -1;
			*lp = s;
			*np = m;
		}
		(*lp)[n++] = c;
		if(c == delim)
			break;
	}
	if(n == 0 && (c == EOF || ferror(f)))
		return -1;
	(*lp)[n] = '\0';
	return n;
}

ssize_t
getline(char **lp, size_t *np, FILE *f)
{
	return getdelim(lp, np, '\n', f);
}
