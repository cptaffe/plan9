/* POSIX's basename and dirname (<libgen.h>): trailing slashes are not an element */
#include <libgen.h>
#include <string.h>

char*
basename(char *p)
{
	char *e, *s;

	if(p == NULL || *p == '\0')
		return ".";
	e = p + strlen(p);
	while(e > p+1 && e[-1] == '/')
		*--e = '\0';
	if((s = strrchr(p, '/')) == NULL || s[1] == '\0')
		return p;
	return s+1;
}

char*
dirname(char *p)
{
	char *e;

	if(p == NULL || *p == '\0')
		return ".";
	e = p + strlen(p);
	while(e > p+1 && e[-1] == '/')	/* trailing slashes */
		e--;
	while(e > p && e[-1] != '/')	/* the last element */
		e--;
	if(e == p)
		return *p == '/' ? "/" : ".";
	while(e > p+1 && e[-1] == '/')	/* the slashes before it */
		e--;
	*e = '\0';
	return p;
}
