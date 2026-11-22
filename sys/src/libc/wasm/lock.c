#include <u.h>
#include <libc.h>

void
lock(Lock *l)
{
	if(ainc(&l->key) == 1)
		return;
	while(semacquire(&l->sem, 1) < 0)
		;
}

void
unlock(Lock *l)
{
	if(adec(&l->key) == 0)
		return;
	semrelease(&l->sem, 1);
}

int
canlock(Lock *l)
{
	return cas32((u32int*)&l->key, 0, 1);
}
