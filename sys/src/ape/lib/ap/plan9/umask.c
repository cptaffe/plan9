#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * The process's own: open, creat and mkdir clear its bits from a new file's mode
 */
mode_t	_umask;

mode_t
umask(mode_t m)
{
	mode_t o;

	o = _umask;
	_umask = m & 0777;
	return o;
}
