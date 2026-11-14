#include "lib.h"
#include <unistd.h>
#include "sys9.h"

unsigned int
alarm(unsigned seconds)
{
	_itimerms = 0;
	return _ALARM(seconds*1000);
}
