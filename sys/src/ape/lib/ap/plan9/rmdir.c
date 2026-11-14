#include "lib.h"
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "sys9.h"
#include "dir.h"

int
rmdir(const char *path)
{
	Dir *d;

	if((d = _dirstat(path)) == nil){
		_syserrno();
		return -1;
	}
	if(!(d->mode&DMDIR)){
		free(d);
		errno = ENOTDIR;
		return -1;
	}
	free(d);
	if(_REMOVE(path) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}
