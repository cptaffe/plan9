#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

extern void _finish(int, char *);

void
abort(void)
{
	sigset_t s;

	sigemptyset(&s);
	sigaddset(&s, SIGABRT);
	sigprocmask(SIG_UNBLOCK, &s, NULL);
	raise(SIGABRT);
	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);
	_finish(0, "abort");
}
