#include <u.h>
#include <libc.h>

int
islocal(char *name)
{
	char *s;
	int ok;

	if(name[0] == 0 || name[0] == '/' || name[0] == '#')
		return 0;
	s = strdup(name);
	if(s == nil)
		return 0;
	cleanname(s);
	ok = strcmp(s, "..") != 0 && strncmp(s, "../", 3) != 0;
	free(s);
	return ok;
}
