#include <signal.h>
#include <string.h>
#include <stdio.h>

static char *names[] = {
	[SIGHUP]	"Hangup",
	[SIGINT]	"Interrupt",
	[SIGQUIT]	"Quit",
	[SIGILL]	"Illegal instruction",
	[SIGABRT]	"Abort",
	[SIGFPE]	"Floating point exception",
	[SIGKILL]	"Killed",
	[SIGSEGV]	"Segmentation fault",
	[SIGPIPE]	"Broken pipe",
	[SIGALRM]	"Alarm clock",
	[SIGTERM]	"Terminated",
	[SIGUSR1]	"User defined signal 1",
	[SIGUSR2]	"User defined signal 2",
	[SIGBUS]	"Bus error",
	[SIGCHLD]	"Child exited",
	[SIGCONT]	"Continued",
	[SIGSTOP]	"Stopped (signal)",
	[SIGTSTP]	"Stopped",
	[SIGTTIN]	"Stopped (tty input)",
	[SIGTTOU]	"Stopped (tty output)",
	[SIGWINCH]	"Window changed",
	[SIGTRAP]	"Trace trap",
	[SIGSYS]	"Bad system call",
	[SIGURG]	"Urgent I/O condition",
	[SIGIO]		"I/O possible",
	[SIGPROF]	"Profiling timer expired",
	[SIGVTALRM]	"Virtual timer expired",
	[SIGXCPU]	"CPU time limit exceeded",
	[SIGXFSZ]	"File size limit exceeded",
};

char*
strsignal(int sig)
{
	static char buf[32];

	if(sig > 0 && sig < sizeof names/sizeof names[0] && names[sig])
		return names[sig];
	snprintf(buf, sizeof buf, "Unknown signal %d", sig);
	return buf;
}
