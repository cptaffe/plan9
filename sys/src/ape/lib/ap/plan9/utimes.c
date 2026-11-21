#include "lib.h"
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include "dir.h"

int
utimes(const char *path, const struct timeval tv[2])
{
	Dir d;

	_nulldir(&d);
	if(tv == NULL)
		d.atime = d.mtime = time(0);
	else{
		if(tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 || tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000){
			errno = EINVAL;
			return -1;
		}
		d.atime = tv[0].tv_sec;
		d.mtime = tv[1].tv_sec;
	}
	if(_dirwstat((char*)path, &d) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}
