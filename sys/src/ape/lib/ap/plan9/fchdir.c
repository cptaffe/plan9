#include "lib.h"
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "sys9.h"

int
fchdir(int fd)
{
	char dir[8192];
	struct stat s;

	if(fstat(fd, &s) < 0)
		return -1;
	if(!S_ISDIR(s.st_mode)){
		errno = ENOTDIR;
		return -1;
	}
	if(_FD2PATH(fd, dir, sizeof dir) < 0){
		_syserrno();
		return -1;
	}
	return chdir(dir);
}
