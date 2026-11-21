#include "lib.h"
#include <signal.h>

int
sigpending(sigset_t *set)
{
	*set = _psigpending;
	return 0;
}
