#include "lib.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sys9.h"

int
fstatat(int fd, const char *path, struct stat *s, int flag)
{
	char buf[8192];
	int n;

	if(flag & ~AT_SYMLINK_NOFOLLOW){
		errno = EINVAL;
		return -1;
	}
	if(path[0] == '/' || fd == AT_FDCWD)
		return stat(path, s);
	if(path[0] == 0){
		errno = ENOENT;
		return -1;
	}
	if(_FD2PATH(fd, buf, sizeof buf) < 0){
		errno = EBADF;
		return -1;
	}
	n = strlen(buf);
	if(n + 1 + strlen(path) + 1 > sizeof buf){
		errno = ENAMETOOLONG;
		return -1;
	}
	buf[n] = '/';
	strcpy(buf+n+1, path);
	return stat(buf, s);
}
