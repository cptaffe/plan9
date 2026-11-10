#include "lib.h"
#include <unistd.h>
#include <errno.h>
#include "sys9.h"

/* POSIX's (2024): Plan 9's /dev/random, at most 256 bytes */
int
getentropy(void *buf, size_t n)
{
	static int f = -1;
	char *p;
	long r;

	if(n > 256){
		errno = EINVAL;
		return -1;
	}
	if(f < 0 && (f = _OPEN("/dev/random", OREAD|OCEXEC)) < 0){
		_syserrno();
		return -1;
	}
	for(p = buf; n > 0; p += r, n -= r){
		r = _READ(f, p, n);
		if(r <= 0){
			if(r == 0)
				errno = EIO;
			else
				_syserrno();
			_CLOSE(f);
			f = -1;
			return -1;
		}
	}
	return 0;
}
