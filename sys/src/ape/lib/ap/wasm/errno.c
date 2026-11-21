#include <errno.h>
#undef errno

_Thread_local int errno;

int*
__errno(void)
{
	return &errno;
}
