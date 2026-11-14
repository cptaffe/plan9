#include <dirent.h>

int
dirfd(DIR *d)
{
	return d->dd_fd;
}
