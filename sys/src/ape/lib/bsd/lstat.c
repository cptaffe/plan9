#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int
lstat(char *name, struct stat *ans)
{
	return stat(name, ans);
}

int
symlink(const char *, const char *)
{
	errno = EPERM;
	return -1;
}

ssize_t
readlink(const char *, char *, size_t)
{
	errno = EIO;
	return -1;
}
