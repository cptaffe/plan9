#include "lib.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "sys9.h"
#include "dir.h"

int
fsync(int fd)
{
	Dir d;

	if(fd<0 || fd>=OPEN_MAX || !(_fdinfo[fd].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	_nulldir(&d);
	if(_dirfwstat(fd, &d) < 0){
		_syserrno();
		if(errno == EIO)
			return -1;
	}
	return 0;
}
