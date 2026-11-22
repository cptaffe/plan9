#include "../plan9/lib.h"
#include "../plan9/sys9.h"
#define _LOCK_EXTENSION
#include <lock.h>
//#include <lib9.h>

void
lock(Lock *l)
{
	if(ainc(&l->key) == 1)
		return;	/* changed from 0 -> 1: we hold lock */
	/* otherwise wait in kernel */
	while(_SEMACQUIRE(&l->sem, 1) < 0){
		/* interrupted; try again */
	}
}

void
unlock(Lock *l)
{
	if(adec(&l->key) == 0)
		return;	/* changed from 1 -> 0: no contention */
	_SEMRELEASE(&l->sem, 1);
}

int
canlock(Lock *l)
{
	return cas32((unsigned long*)&l->key, 0, 1);
}
