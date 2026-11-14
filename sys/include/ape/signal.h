#ifndef __SIGNAL_H
#define __SIGNAL_H
#pragma lib "/$M/lib/ape/libap.a"

typedef int sig_atomic_t;

/*
 * We don't give arg types for signal handlers, in spite of ANSI requirement
 * that it be 'int' (the signal number), because some programs need an
 * additional context argument.  So the real type of signal handlers is
 *      void handler(int sig, char *, struct Ureg *)
 * where the char * is the Plan 9 message and Ureg is defined in <ureg.h>
 */
#define SIG_DFL ((void (*)())0)
#define SIG_ERR ((void (*)())-1)
#define SIG_IGN ((void (*)())1)

#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught)*/
#define SIGABRT 5	/* used by abort */
#define	SIGFPE	6	/* floating point exception */
#define	SIGKILL	7	/* kill (cannot be caught or ignored) */
#define	SIGSEGV	8	/* segmentation violation */
#define	SIGPIPE	9	/* write on a pipe with no one to read it */
#define	SIGALRM	10	/* alarm clock */
#define	SIGTERM	11	/* software termination signal from kill */
#define	SIGUSR1	12	/* user defined signal 1 */
#define	SIGUSR2	13	/* user defined signal 2 */
#define	SIGBUS	14	/* bus error */

/* The following symbols must be defined, but the signals needn't be supported */
#define SIGCHLD	15	/* child process terminated or stopped */
#define SIGCONT 16	/* continue if stopped */
#define SIGSTOP 17	/* stop */
#define SIGTSTP	18	/* interactive stop */
#define SIGTTIN	19	/* read from ctl tty by member of background */
#define SIGTTOU	20	/* write to ctl tty by member of background */
#define SIGWINCH	21	/* window size change */
#define SIGTRAP	22	/* trace trap */
#define SIGSYS	23	/* bad system call */
#define SIGURG	24
#define SIGIO	25
#define SIGPROF	26
#define SIGVTALRM	27
#define SIGXCPU	28
#define SIGXFSZ	29

#ifdef _BSD_EXTENSION
#define NSIG 30
extern const char *const sys_signame[NSIG];
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void (*signal(int, void (*)()))();
extern int raise(int);

#ifdef __cplusplus
}
#endif

#ifdef _POSIX_SOURCE

typedef long sigset_t;
struct sigaction {
	void		(*sa_handler)();
	sigset_t	sa_mask;
	int		sa_flags;
};
/* values for sa_flags */
#define SA_NOCLDSTOP	1
/*
 * A handler runs after the note is done with (noted), so a signal
 * is never held while its handler runs: SA_NODEFER is always so.
 * A call a note interrupts fails with EINTR, SA_RESTART or not.
 */
#if _POSIX_C_SOURCE >= 200809L || defined(_SUSV2_SOURCE)
#define SA_NODEFER	2
#define SA_RESTART	4
#endif
/* XSI: there is no sigaltstack, so no other stack: a handler runs where it would */
#ifdef _SUSV2_SOURCE
#define SA_ONSTACK	8
#endif

/* first argument to sigprocmask */
#define SIG_BLOCK	1
#define SIG_UNBLOCK	2
#define SIG_SETMASK	3

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __TYPES_H
extern int kill(pid_t, int);
#endif
extern int sigemptyset(sigset_t *);
extern int sigfillset(sigset_t *);
extern int sigaddset(sigset_t *, int);
extern int sigdelset(sigset_t *, int);
extern int sigismember(const sigset_t *, int);
extern int sigaction(int, const struct sigaction *, struct sigaction *);
extern int sigprocmask(int, sigset_t *, sigset_t *);
extern int sigpending(sigset_t *);
extern int sigsuspend(const sigset_t *);

#ifdef __cplusplus
}
#endif

#endif /* _POSIX_SOURCE */

#endif /* __SIGNAL_H */
