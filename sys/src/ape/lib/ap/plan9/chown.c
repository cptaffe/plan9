#include "lib.h"
#include "sys9.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "dir.h"

static int
owners(Dir *d, uid_t owner, gid_t group)
{
	int num;

	_nulldir(d);
	d->uid = nil;
	num = owner;
	if(!_getpw(&num, &d->uid, 0)){
		errno = EINVAL;
		return -1;
	}
	d->gid = nil;
	num = group;
	if(!_getpw(&num, &d->gid, 0)){
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int
chown(const char *path, uid_t owner, gid_t group)
{
	Dir d;

	if(owners(&d, owner, group) < 0)
		return -1;
	if(_dirwstat(path, &d) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}

int
lchown(const char *path, uid_t owner, gid_t group)
{
	return chown(path, owner, group);
}

int
fchown(int fd, uid_t owner, gid_t group)
{
	Dir d;

	if(owners(&d, owner, group) < 0)
		return -1;
	if(_dirfwstat(fd, &d) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}
