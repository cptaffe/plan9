#include "lib.h"
#include <signal.h>
#include <errno.h>
#include <stddef.h>

#define BITSIG(s) (2<<(s))

_TLS sigset_t _psigblocked;
_TLS sigset_t _psigpending;

void
_sigdeliver(void)
{
	sigset_t p;
	int s;

	while((p = _psigpending & ~_psigblocked) != 0)
		for(s = 1; s <= MAXSIG; s++)
			if(p & BITSIG(s)){
				_psigpending &= ~BITSIG(s);
				raise(s);
				break;
			}
}

static int
mask(int how, const sigset_t *set, sigset_t *oset)
{
	if(oset)
		*oset = _psigblocked;
	if(set == NULL)
		return 0;
	if(how==SIG_BLOCK)
		_psigblocked |= *set;
	else if(how==SIG_UNBLOCK)
		_psigblocked &= ~*set;
	else if(how==SIG_SETMASK)
		_psigblocked = *set;
	else
		return EINVAL;
	_psigblocked &= ~(BITSIG(SIGKILL)|BITSIG(SIGSTOP));
	return 0;
}

int
sigprocmask(int how, sigset_t *set, sigset_t *oset)
{
	int e;

	if((e = mask(how, set, oset)) != 0){
		errno = e;
		return -1;
	}
	_sigdeliver();
	return 0;
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	int e;

	if((e = mask(how, set, oset)) == 0)
		_sigdeliver();
	return e;
}
