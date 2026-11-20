#include "lib.h"
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int
unlinkat(int fd, const char *path, int flag)
{
	char buf[8192], *p;

	if(flag & ~AT_REMOVEDIR){
		errno = EINVAL;
		return -1;
	}
	if((p = _atpath(fd, path, buf, sizeof buf)) == NULL)
		return -1;
	if(flag & AT_REMOVEDIR)
		return rmdir(p);
	return unlink(p);
}
