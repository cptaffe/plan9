#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

extern void _finish(int, char *);

void
abort(void)
{
	kill(getpid(), SIGABRT);
	signal(SIGABRT, SIG_DFL);
	kill(getpid(), SIGABRT);
	_finish(0, "abort");
}
