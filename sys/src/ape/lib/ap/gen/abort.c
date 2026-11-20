#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

void
abort(void)
{
	sigset_t s;

	sigemptyset(&s);
	sigaddset(&s, SIGABRT);
	sigprocmask(SIG_UNBLOCK, &s, NULL);
	raise(SIGABRT);
}
