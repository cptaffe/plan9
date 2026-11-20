#include "lib.h"
#include <sched.h>
#include "sys9.h"

int
sched_yield(void)
{
	_SLEEP(0);
	return 0;
}
