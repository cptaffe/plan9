#include "lib.h"
#include <unistd.h>
#include <errno.h>

int
dup(int oldd)
{
	return fcntl(oldd, F_DUPFD, 0);
}

int
dup2(int oldd, int newd)
{
	if(newd < 0 || newd >= OPEN_MAX){
		errno = EBADF;
		return -1;
	}
	if(oldd == newd && _fdinfo[newd].flags&FD_ISOPEN)
		return newd;
	close(newd);
	if(_bufmove(newd) < 0)
		return -1;
	return fcntl(oldd, F_DUPFD, newd);
}
