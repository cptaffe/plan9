#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/*
 * BUG: only the id the process has can be set
 */

int
setgid(gid_t id)
{
	if(id == getgid())
		return 0;
	errno = EPERM;
	return -1;
}

int
setegid(gid_t id)
{
	if(id == getegid())
		return 0;
	errno = EPERM;
	return -1;
}
