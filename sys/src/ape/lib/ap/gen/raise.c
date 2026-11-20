/* not a posix function, but implemented with posix kill, getpid */

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

extern pid_t _getpid(void);

int
raise(int sig)
{
	return kill(_getpid(), sig);
}
