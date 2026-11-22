#include "lib.h"
#include "sys9.h"
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

static void
siginfotramp(int sig)
{
	siginfo_t si;

	memset(&si, 0, sizeof si);
	si.si_signo = sig;
	si.si_code = SI_USER;
	(*sigact[sig])(sig, &si, nil);
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

	if(sig <= 0 || sig > MAXSIG){
		errno = EINVAL;
		return SIG_ERR;
	}
	oldf = _sighdlr[sig];
	if(sig == SIGKILL)
		return oldf;	/* can't catch or ignore SIGKILL */
	_sighdlr[sig] = func;
	sigflags[sig] = 0;
	discard(sig, func);
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
	if(sig <= 0 || sig > MAXSIG || sig == SIGKILL){
		errno = EINVAL;
		return -1;
	}
	if(oact){
		if(sigflags[sig] & SA_SIGINFO)
			oact->sa_sigaction = sigact[sig];
		else
			oact->sa_handler = _sighdlr[sig];
		oact->sa_mask = _psigblocked;
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
	return 0;
}

int (*_thrdie)(char*);

/* this is registered in _envsetup */
int
_notehandler(void *u, char *msg)
{
	int i;
	void(*f)(int, char*, Ureg*);
	extern void _doatexits(void);	/* in stdio/exit.c */

	if(_finishing)
		_finish(0, 0);
	if(_itimerms > 0 && strncmp(msg, "alarm", 5) == 0)
		_ALARM(_itimerms);
	for(i = 0; i<NSIGTAB; i++){
		if(strncmp(msg, sigtab[i].msg, strlen(sigtab[i].msg)) == 0){
			if(_psigblocked & 2<<sigtab[i].num){
				_psigpending |= 2<<sigtab[i].num;
				_NOTED(0); /* NCONT */
				return 0;
			}
			f = _sighdlr[sigtab[i].num];
			if(f == SIG_DFL && dflign(sigtab[i].num))
				f = SIG_IGN;
			if(f == SIG_DFL || f == SIG_ERR)
				break;
			if(f != SIG_IGN) {
				if(sigflags[sigtab[i].num] & SA_RESETHAND){
					_sighdlr[sigtab[i].num] = SIG_DFL;
					sigflags[sigtab[i].num] &= ~SA_RESETHAND;
				}
				_notetramp(sigtab[i].num, f, u);
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
