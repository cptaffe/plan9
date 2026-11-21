#include "lib.h"
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

int
fstatat(int fd, const char *path, struct stat *s, int flag)
{
	char buf[8192], *p;

	if(flag & ~AT_SYMLINK_NOFOLLOW){
		errno = EINVAL;
		return -1;
	}
	if((p = _atpath(fd, path, buf, sizeof buf)) == NULL)
		return -1;
	return stat(p, s);
}
