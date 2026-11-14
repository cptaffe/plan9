#include "lib.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int
fsync(int fd)
{
	if(fd<0 || fd>=OPEN_MAX || !(_fdinfo[fd].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	return 0;
}
