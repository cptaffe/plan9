#include <errno.h>
#include <unistd.h>
#include "lib.h"
#include "sys9.h"

ssize_t
pread(int d, void *buf, size_t n, off_t off)
{
	long r;

	if(d<0 || d>=OPEN_MAX || !(_fdinfo[d].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	if(off < 0){
		errno = EINVAL;
		return -1;
	}
	if((r = _PREAD(d, buf, n, off)) < 0)
		_syserrno();
	return r;
}

ssize_t
pwrite(int d, const void *buf, size_t n, off_t off)
{
	long r;

	if(d<0 || d>=OPEN_MAX || !(_fdinfo[d].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	if(off < 0){
		errno = EINVAL;
		return -1;
	}
	if((r = _PWRITE(d, (void*)buf, n, off)) < 0)
		_syserrno();
	return r;
}
