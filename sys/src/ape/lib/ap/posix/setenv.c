#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * POSIX's: environ is the environment (execve writes it to the new
 * /env), so these change environ.  A new environ is ours, malloc'd;
 * the one _envsetup made, or a program set, is not freed.
 */
extern char **environ;
static char **ours;

static int
bad(const char *name)
{
	if(name == NULL || *name == 0 || strchr(name, '=') != NULL){
		errno = EINVAL;
		return 1;
	}
	return 0;
}

static char**
find(const char *name, size_t n)
{
	char **p;

	for(p = environ; *p != NULL; p++)
		if(strncmp(*p, name, n) == 0 && (*p)[n] == '=')
			return p;
	return NULL;
}

int
setenv(const char *name, const char *value, int overwrite)
{
	size_t n, m, i;
	char *s, **p, **e;

	if(bad(name))
		return -1;
	n = strlen(name);
	p = find(name, n);
	if(p != NULL && !overwrite)
		return 0;
	m = strlen(value);
	if((s = malloc(n+1+m+1)) == NULL){
		errno = ENOMEM;
		return -1;
	}
	memcpy(s, name, n);
	s[n] = '=';
	memcpy(s+n+1, value, m+1);
	if(p != NULL){
		*p = s;
		return 0;
	}
	for(i = 0; environ[i] != NULL; i++)
		;
	if((e = malloc((i+2)*sizeof e[0])) == NULL){
		free(s);
		errno = ENOMEM;
		return -1;
	}
	memcpy(e, environ, i*sizeof e[0]);
	e[i] = s;
	e[i+1] = NULL;
	if(environ == ours)
		free(ours);
	environ = ours = e;
	return 0;
}

int
unsetenv(const char *name)
{
	size_t n;
	char **p, **q;

	if(bad(name))
		return -1;
	n = strlen(name);
	while((p = find(name, n)) != NULL)
		for(q = p; (*q = q[1]) != NULL; q++)
			;
	return 0;
}
