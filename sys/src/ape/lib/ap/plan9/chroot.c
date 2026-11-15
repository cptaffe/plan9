#include "lib.h"
#include <unistd.h>
#include <errno.h>
#include "sys9.h"

int
chroot(const char *path)
{
	if(_RFORK(RFNAMEG) < 0 || _BIND(path, "/", MREPL) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}
