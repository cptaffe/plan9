#include <signal.h>
#include <errno.h>

/*
 * sigsets are 32-bit longs.  if the 2<<(i-1) bit is on,
 * the signal #define'd as i in signal.h is inluded.
 */

#define BITSIG(s) (2<<(s))

static sigset_t stdsigs = BITSIG(SIGHUP)|BITSIG(SIGINT)|BITSIG(SIGQUIT)|BITSIG(SIGILL)|
		BITSIG(SIGABRT)|BITSIG(SIGFPE)|BITSIG(SIGKILL)|BITSIG(SIGSEGV)|BITSIG(SIGPIPE)|
		BITSIG(SIGALRM)|BITSIG(SIGTERM)|BITSIG(SIGUSR1)|BITSIG(SIGUSR2)|BITSIG(SIGBUS)|
		BITSIG(SIGCHLD)|BITSIG(SIGCONT)|BITSIG(SIGSTOP)|BITSIG(SIGTSTP)|BITSIG(SIGTTIN)|
		BITSIG(SIGTTOU)|BITSIG(SIGWINCH)|BITSIG(SIGTRAP)|BITSIG(SIGSYS)|BITSIG(SIGURG)|
		BITSIG(SIGIO)|BITSIG(SIGPROF)|BITSIG(SIGVTALRM)|BITSIG(SIGXCPU)|BITSIG(SIGXFSZ);

int
sigemptyset(sigset_t *set)
{
	*set = 0;
	return 0;
}

int
sigfillset(sigset_t *set)
{
	*set = stdsigs;
	return 0;
}

int
sigaddset(sigset_t *set, int signo)
{
	int b;

	b = BITSIG(signo);
	if(!(b&stdsigs)){
		errno = EINVAL;
		return -1;
	}
	*set |= b;
	return 0;
}

int
sigdelset(sigset_t *set, int signo)
{
	int b;

	b = BITSIG(signo);
	if(!(b&stdsigs)){
		errno = EINVAL;
		return -1;
	}
	*set &= ~b;
	return 0;
}

int
sigismember(const sigset_t *set, int signo)
{
	int b;

	b = BITSIG(signo);
	if(!(b&stdsigs)){
		errno = EINVAL;
		return -1;
	}
	return (b&*set)? 1 : 0;
}
