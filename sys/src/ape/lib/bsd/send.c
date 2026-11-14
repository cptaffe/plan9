/* posix */
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* bsd extensions */
#include <sys/uio.h>
#include <sys/socket.h>

#include "priv.h"
#include "lib.h"

int
send(int fd, void *a, int n, int flags)
{
	if(flags & MSG_OOB){
		errno = EOPNOTSUPP;
		return -1;
	}
	return write(fd, a, n);
}

int
recv(int fd, void *a, int n, int flags)
{
	Fdinfo *f;

	if(flags & MSG_OOB){
		errno = EOPNOTSUPP;
		return -1;
	}
	if(!(flags & MSG_PEEK))
		return read(fd, a, n);
	if(fd < 0 || fd >= OPEN_MAX || !(_fdinfo[fd].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	f = &_fdinfo[fd];
	if((f->flags&FD_BUFFEREDX) || !(f->flags&FD_BUFFERED) && _startbuf(fd) != 0){
		errno = EIO;
		return -1;
	}
	return _peekbuf(fd, a, n, f->oflags&O_NONBLOCK);
}
