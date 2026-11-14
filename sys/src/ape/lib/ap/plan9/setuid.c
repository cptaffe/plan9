#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/*
 * BUG: only the id the process has can be set
 */

int
setuid(uid_t id)
{
	if(id == getuid())
		return 0;
	errno = EPERM;
	return -1;
}

int
seteuid(uid_t id)
{
	if(id == geteuid())
		return 0;
	errno = EPERM;
	return -1;
}
