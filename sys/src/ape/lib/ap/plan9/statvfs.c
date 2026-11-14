#include "lib.h"
#include <sys/statvfs.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "sys9.h"
#include "dir.h"

static int
fill(Dir *d, struct statvfs *buf)
{
	if(d == nil){
		_syserrno();
		return -1;
	}
	memset(buf, 0, sizeof *buf);
	buf->f_bsize = 8192;
	buf->f_frsize = 8192;
	buf->f_fsid = d->type<<16 | d->dev;
	buf->f_namemax = 255;
	free(d);
	return 0;
}

int
statvfs(const char *path, struct statvfs *buf)
{
	return fill(_dirstat(path), buf);
}

int
fstatvfs(int fd, struct statvfs *buf)
{
	return fill(_dirfstat(fd), buf);
}
