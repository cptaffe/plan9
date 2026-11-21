#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include "thr.h"

int (*_thrkill)(pthread_t, int);

int
pthread_kill(pthread_t id, int sig)
{
	if(sig < 0 || sig >= Nsig)
		return EINVAL;
	if(_thrkill != NULL)
		return _thrkill(id, sig);
	if(sig == 0)
		return 0;
	return raise(sig) < 0? errno: 0;
}
