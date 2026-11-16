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
readlink(const char *name, char *, size_t)
{
	struct stat s;

	if(stat(name, &s) < 0)
		return -1;
	errno = EINVAL;
	return -1;
}
