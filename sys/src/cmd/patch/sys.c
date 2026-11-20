#include <u.h>
#include <libc.h>

int
pcreate(char *f, int mode, ulong perm)
{
	return create(f, mode, perm);
}

int
premove(char *f)
{
	return remove(f);
}

int
pwstat(char *f, Dir *d)
{
	return dirwstat(f, d);
}
