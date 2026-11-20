#include "lib.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "sys9.h"

char*
_atpath(int fd, const char *path, char *buf, int nbuf)
{
	int n;

	if(path[0] == '/' || fd == AT_FDCWD)
		return (char*)path;
	if(path[0] == 0){
		errno = ENOENT;
		return NULL;
	}
	if(_FD2PATH(fd, buf, nbuf) < 0){
		errno = EBADF;
		return NULL;
	}
	n = strlen(buf);
	if(n + 1 + strlen(path) + 1 > nbuf){
		errno = ENAMETOOLONG;
		return NULL;
	}
	buf[n] = '/';
	strcpy(buf+n+1, path);
	return buf;
}
