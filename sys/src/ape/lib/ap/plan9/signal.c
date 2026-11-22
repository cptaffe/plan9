#define _LOCK_EXTENSION
#include "lib.h"
#include "sys9.h"
#include <lock.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>

long _itimerms;

static struct {
	char	*msg;	/* just check prefix */
	int	num;
} sigtab[] = {
	{"hangup",				SIGHUP},
	{"interrupt",				SIGINT},
	{"quit",				SIGQUIT},
	{"alarm",				SIGALRM},
	{"sys: trap: illegal instruction",	SIGILL},
	{"sys: trap: reserved instruction",	SIGILL},
	{"sys: trap: reserved",			SIGILL},
	{"sys: trap: arithmetic overflow",	SIGFPE},
	{"abort",				SIGABRT},
	{"sys: fp:",				SIGFPE},
	{"exit",				SIGKILL},
	{"die",					SIGKILL},
	{"kill",				SIGKILL},
	{"sys: trap: bus error",		SIGSEGV},
	{"sys: trap: address error",		SIGSEGV},
	{"sys: trap: TLB",			SIGSEGV},
	{"sys: write on closed pipe",		SIGPIPE},
	{"alarm",				SIGALRM},
	{"term",				SIGTERM},
	{"usr1",				SIGUSR1},
	{"usr2",				SIGUSR2},
	{"winch",				SIGWINCH},
	{"sigbus",				SIGBUS},
	{"sigchld",				SIGCHLD},
	{"sigcont",				SIGCONT},
	{"sigstop",				SIGSTOP},
	{"sigtstp",				SIGTSTP},
	{"sigttin",				SIGTTIN},
	{"sigttou",				SIGTTOU},
	{"sigtrap",				SIGTRAP},
	{"sigsys",				SIGSYS},
	{"sigurg",				SIGURG},
	{"sigio",				SIGIO},
	{"sigprof",				SIGPROF},
	{"sigvtalrm",				SIGVTALRM},
	{"sigxcpu",				SIGXCPU},
	{"sigxfsz",				SIGXFSZ},
};
#define NSIGTAB ((sizeof sigtab)/(sizeof (sigtab[0])))

void	(*_sighdlr[MAXSIG+1])(int, char*, Ureg*); /* 0 initialized: SIG_DFL */
static int	sigflags[MAXSIG+1];
static void	(*sigact[MAXSIG+1])(int, siginfo_t*, void*);
static Lock	siglk[MAXSIG+1];
static _TLS void	(*sigcur[MAXSIG+1])(int, siginfo_t*, void*);

static void
siginfotramp(int sig)
{
	siginfo_t si;

	memset(&si, 0, sizeof si);
	si.si_signo = sig;
	si.si_code = SI_USER;
	(*sigcur[sig])(sig, &si, nil);
}

static void
siglock(int sig)
{
	while(!canlock(&siglk[sig]))
		_SLEEP(0);
}

static sigset_t
take(int sig)
{
	sigset_t m;

	m = _psigblocked;
	_psigblocked = ~0;
	siglock(sig);
	return m;
}

static void
give(int sig, sigset_t m)
{
	unlock(&siglk[sig]);
	_psigblocked = m;
	_sigdeliver();
}

static int
dflign(int sig)
{
	switch(sig){
	case SIGCHLD:
	case SIGCONT:
	case SIGURG:
	case SIGWINCH:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		return 1;
	}
	return 0;
}

static void
discard(int sig, void (*f)())
{
	if(f == SIG_IGN || f == SIG_DFL && dflign(sig))
		_psigpending &= ~(2<<sig);
}

/* must match signal.h: extern void (*signal(int, void (*)()))(); */
//void (*signal(int sig, void (*func)(int, char*, Ureg*)))(int, char*, Ureg*)
void
(*signal(int sig, void (*func)()))()
{
	void(*oldf)(int, char*, Ureg*);
	sigset_t m;

	if(sig <= 0 || sig > MAXSIG){
		errno = EINVAL;
		return SIG_ERR;
	}
	if(sig == SIGKILL)
		return _sighdlr[sig];	/* can't catch or ignore SIGKILL */
	m = take(sig);
	oldf = _sighdlr[sig];
	_sighdlr[sig] = func;
	sigflags[sig] = 0;
	discard(sig, func);
	give(sig, m);
	return oldf;
}

/* BAD CODE - see /sys/src/ape/lib/ap/$objtype/setjmp.s for real code
int
sigsetjmp(sigjmp_buf buf, int savemask)
{
	int r;

	buf[0] = savemask;
	buf[1] = _psigblocked;
	return setjmp(&buf[2]);
}
*/

/*
 * BUG: improper handling of process signal mask
 */
int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	sigset_t m;

	if(sig <= 0 || sig > MAXSIG || sig == SIGKILL){
		errno = EINVAL;
		return -1;
	}
	m = take(sig);
	if(oact){
		if(sigflags[sig] & SA_SIGINFO)
			oact->sa_sigaction = sigact[sig];
		else
			oact->sa_handler = _sighdlr[sig];
		oact->sa_mask = m;
		oact->sa_flags = sigflags[sig];
	}
	if(act){
		sigflags[sig] = act->sa_flags & (SA_RESETHAND|SA_SIGINFO);
		if(act->sa_flags & SA_SIGINFO){
			sigact[sig] = act->sa_sigaction;
			_sighdlr[sig] = (void(*)(int, char*, Ureg*))siginfotramp;
		}else
			_sighdlr[sig] = act->sa_handler;
		discard(sig, _sighdlr[sig]);
	}
	give(sig, m);
	return 0;
}

int (*_thrdie)(char*);

/* this is registered in _envsetup */
int
_notehandler(void *u, char *msg)
{
	int i, s;
	void(*f)(int, char*, Ureg*);
	extern void _doatexits(void);	/* in stdio/exit.c */

	if(_finishing)
		_finish(0, 0);
	if(_itimerms > 0 && strncmp(msg, "alarm", 5) == 0)
		_ALARM(_itimerms);
	for(i = 0; i<NSIGTAB; i++){
		if(strncmp(msg, sigtab[i].msg, strlen(sigtab[i].msg)) == 0){
			s = sigtab[i].num;
			if(_psigblocked & 2<<s){
				_psigpending |= 2<<s;
				_NOTED(0); /* NCONT */
				return 0;
			}
			siglock(s);
			f = _sighdlr[s];
			sigcur[s] = sigact[s];
			if(f != SIG_DFL && f != SIG_IGN && f != SIG_ERR && sigflags[s] & SA_RESETHAND){
				_sighdlr[s] = SIG_DFL;
				sigflags[s] = 0;
			}
			unlock(&siglk[s]);
			if(f == SIG_DFL && dflign(s))
				f = SIG_IGN;
			if(f == SIG_DFL || f == SIG_ERR)
				break;
			if(f != SIG_IGN) {
				_notetramp(s, f, u);
				/* notetramp is machine-dependent; doesn't return to here */
			}
			_NOTED(0); /* NCONT */
			return 0;
		}
	}
	if((_thrdie == NULL || !_thrdie(msg)) && (i == NSIGTAB || sigtab[i].num != SIGABRT))
		_doatexits();
	_NOTED(1); /* NDFLT */
	return 0;
}

int
_stringsig(char *nam)
{
	int i;

	for(i = 0; i<NSIGTAB; i++)
		if(strncmp(nam, sigtab[i].msg, strlen(sigtab[i].msg)) == 0)
			return sigtab[i].num;
	return 0;
}

char *
_sigstring(int sig)
{
	int i;

	for(i=0; i<NSIGTAB; i++)
		if(sigtab[i].num == sig)
			return sigtab[i].msg;
	return "unknown signal";
}
