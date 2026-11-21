#include "lib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "dir.h"

static int
dtimes(Dir *d, const struct timespec ts[2])
{
	ulong *f[2];
	int i;

	_nulldir(d);
	f[0] = &d->atime;
	f[1] = &d->mtime;
	for(i = 0; i < 2; i++){
		if(ts == NULL || ts[i].tv_nsec == UTIME_NOW)
			*f[i] = time(0);
		else if(ts[i].tv_nsec == UTIME_OMIT)
			continue;
		else if(ts[i].tv_nsec < 0 || ts[i].tv_nsec >= 1000000000){
			errno = EINVAL;
			return -1;
		}else
			*f[i] = ts[i].tv_sec;
	}
	return 0;
}

int
utimensat(int fd, const char *path, const struct timespec ts[2], int flag)
{
	char buf[8192], *p;
	Dir d;

	if(flag & ~AT_SYMLINK_NOFOLLOW){
		errno = EINVAL;
		return -1;
	}
	if((p = _atpath(fd, path, buf, sizeof buf)) == NULL || dtimes(&d, ts) < 0)
		return -1;
	if(_dirwstat(p, &d) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}

int
futimens(int fd, const struct timespec ts[2])
{
	Dir d;

	if(dtimes(&d, ts) < 0)
		return -1;
	if(_dirfwstat(fd, &d) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}
