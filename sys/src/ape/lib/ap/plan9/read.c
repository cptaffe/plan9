#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "lib.h"
#include "sys9.h"
#include "dir.h"
#include <stdlib.h>

#include <stdio.h>

ssize_t
read(int d, void *buf, size_t nbytes)
{
	int n, noblock, isbuf;
	Fdinfo *f;

	if(d<0 || d>=OPEN_MAX || !(_fdinfo[d].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	if(nbytes <= 0)
		return 0;
	if(buf == 0){
		errno = EFAULT;
		return -1;
	}
	f = &_fdinfo[d];
	if(!(f->flags&FD_DIRCHECKED)){
		Dir *dd;

		f->flags |= FD_DIRCHECKED;
		if((dd = _dirfstat(d)) != nil){
			if(dd->mode&DMDIR)
				f->flags |= FD_ISDIR;
			free(dd);
		}
	}
	if(f->flags&FD_ISDIR){
		errno = EISDIR;
		return -1;
	}
	noblock = f->oflags&O_NONBLOCK;
	isbuf = f->flags&(FD_BUFFERED|FD_BUFFEREDX);
	if(noblock || isbuf){
		if(f->flags&FD_BUFFEREDX) {
			errno = EIO;
			return -1;
		}
		if(!isbuf) {
			if(_startbuf(d) != 0) {
				errno = EIO;
				return -1;
			}
		}
		n = _readbuf(d, buf, nbytes, noblock);
	}else{
		n = _READ(d,  buf, nbytes);
		if(n < 0)
			_syserrno();
	}
	return n;
}
