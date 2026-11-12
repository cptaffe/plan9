/*
 * POSIX's realpath: the absolute name of a file that exists, with no . or .. and no repeated
 * slash. Plan 9 has no symbolic links: the name is the lexical one, from the current directory.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 1024	/* limits.h has none: Plan 9 names have no limit */
#endif

char*
realpath(const char *path, char *resolved)
{
	char buf[PATH_MAX], *r, *e, *s, *t;
	size_t n;
	struct stat st;

	if(path == NULL){
		errno = EINVAL;
		return NULL;
	}
	if(*path == '\0'){
		errno = ENOENT;
		return NULL;
	}
	if(*path == '/')
		buf[0] = '\0';
	else if(getcwd(buf, sizeof buf) == NULL)
		return NULL;
	n = strlen(buf);
	if(n + 1 + strlen(path) + 1 > sizeof buf){
		errno = ENAMETOOLONG;
		return NULL;
	}
	buf[n++] = '/';
	strcpy(buf+n, path);

	/* clean buf in place: r is where the next element goes */
	r = buf+1;
	for(s = buf+1; *s != '\0'; s = e){
		while(*s == '/')
			s++;
		for(e = s; *e != '\0' && *e != '/'; e++)
			;
		if(e == s)
			break;
		if(e - s == 1 && s[0] == '.')
			continue;
		if(e - s == 2 && s[0] == '.' && s[1] == '.'){
			if(r > buf+1){
				for(t = r-1; t > buf+1 && t[-1] != '/'; t--)
					;
				r = t;
			}
			continue;
		}
		memmove(r, s, e - s);
		r += e - s;
		*r++ = '/';
	}
	if(r > buf+1)
		r--;
	*r = '\0';
	if(stat(buf, &st) < 0)
		return NULL;
	if(resolved == NULL && (resolved = malloc(strlen(buf)+1)) == NULL){
		errno = ENOMEM;
		return NULL;
	}
	strcpy(resolved, buf);
	return resolved;
}
