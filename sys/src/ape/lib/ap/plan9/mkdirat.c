#include "lib.h"
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>

int
mkdirat(int fd, const char *path, mode_t mode)
{
	char buf[8192], *p;

	if((p = _atpath(fd, path, buf, sizeof buf)) == NULL)
		return -1;
	return mkdir(p, mode);
}
