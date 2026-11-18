#include "lib.h"
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>

int
openat(int fd, const char *path, int flags, ...)
{
	char buf[8192], *p;
	va_list va;
	mode_t mode;

	mode = 0;
	if(flags & O_CREAT){
		va_start(va, flags);
		mode = va_arg(va, int);
		va_end(va);
	}
	if((p = _atpath(fd, path, buf, sizeof buf)) == NULL)
		return -1;
	return open(p, flags, mode);
}
