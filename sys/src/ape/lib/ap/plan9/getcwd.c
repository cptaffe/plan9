#include "lib.h"
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "sys9.h"
#include "dir.h"

char*
getcwd(char *buf, size_t len)
{
	char path[8192];
	int fd;
	size_t n;

	fd = _OPEN(".", OREAD);
	if(fd < 0) {
		errno = EACCES;
		return 0;
	}
	if(_FD2PATH(fd, path, sizeof path) < 0) {
		errno = EIO;
		_CLOSE(fd);
		return 0;
	}
	_CLOSE(fd);
	if(path[0] == '\0')
		strcpy(path, "/");
	n = strlen(path);
	if(n >= sizeof path - 1) {
		errno = ENAMETOOLONG;
		return 0;
	}
	if(buf == 0) {
		if(len == 0)
			len = n+1;
		if(n+1 > len) {
			errno = ERANGE;
			return 0;
		}
		if((buf = malloc(len)) == 0) {
			errno = ENOMEM;
			return 0;
		}
	}
	if(len == 0 || n+1 > len) {
		errno = len == 0 ? EINVAL : ERANGE;
		return 0;
	}
	memcpy(buf, path, n+1);
	return buf;
}
