/*
 * pANS stdio -- remove
 */
#include "iolib.h"
#include <sys/stat.h>
#include <unistd.h>

int
remove(const char *f)
{
	struct stat s;

	if(stat(f, &s) == 0 && S_ISDIR(s.st_mode))
		return rmdir(f);
	return unlink(f);
}
