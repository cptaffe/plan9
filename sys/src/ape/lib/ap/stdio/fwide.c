/*
 * C95 stdio -- fwide
 *	A stream's orientation is kept, but each stream is bytes,
 *	and a wide one's characters are their UTF-8.
 */
#include "iolib.h"

signed char _IO_orient[FOPEN_MAX];

int
fwide(FILE *f, int mode)
{
	int i;

	i = f - _IO_stream;
	if(i < 0 || i >= FOPEN_MAX)
		return 0;
	if(_IO_orient[i] == 0 && mode != 0)
		_IO_orient[i] = mode > 0? 1: -1;
	return _IO_orient[i];
}
