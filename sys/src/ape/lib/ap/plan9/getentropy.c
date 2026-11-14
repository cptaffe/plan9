#include "lib.h"
#include <unistd.h>
#include <errno.h>
#include "sys9.h"

/* POSIX's (2024): Plan 9's /dev/random, at most 256 bytes */
int
getentropy(void *buf, size_t n)
{
	char *p;
	long r;
	int f;

	if(n > 256){
		errno = EINVAL;
		return -1;
	}
	if((f = _OPEN("/dev/random", OREAD|OCEXEC)) < 0){
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
			return -1;
		}
	}
	_CLOSE(f);
	return 0;
}
