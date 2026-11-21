/*
 * POSIX threads for a process of one thread of control (<pthread.h>): its data by key, once,
 * and mutexes, which no other thread can hold. A thread's end is the process's: pthread_exit
 * runs the keys' destructors, as POSIX's does, and exits.
 */
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static struct {
	int used;
	void (*dtor)(void *);
	void *v;
} keys[PTHREAD_KEYS_MAX];

pthread_t
pthread_self(void)
{
	return 1;
}

int
pthread_equal(pthread_t a, pthread_t b)
{
	return a == b;
}

int
pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg)
{
	return EAGAIN;	/* the process has one thread */
}

int
pthread_join(pthread_t t, void **r)
{
	return t == pthread_self() ? EDEADLK : ESRCH;
}

int
pthread_detach(pthread_t t)
{
	return t == pthread_self() ? 0 : ESRCH;
}

void
pthread_exit(void *r)
{
	int i, n, again;
	void *v;

	for(n = 0; n < PTHREAD_DESTRUCTOR_ITERATIONS; n++){
		again = 0;
		for(i = 0; i < PTHREAD_KEYS_MAX; i++)
			if(keys[i].used && keys[i].dtor != NULL && (v = keys[i].v) != NULL){
				keys[i].v = NULL;
				keys[i].dtor(v);
				again = 1;
			}
		if(!again)
			break;
	}
	exit(0);
}

int pthread_attr_init(pthread_attr_t *a) { a->detached = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { return 0; }

int
pthread_attr_setdetachstate(pthread_attr_t *a, int s)
{
	if(s != PTHREAD_CREATE_JOINABLE && s != PTHREAD_CREATE_DETACHED)
		return EINVAL;
	a->detached = s;
	return 0;
}

int
pthread_once(pthread_once_t *o, void (*f)(void))
{
	if(!o->done){
		o->done = 1;
		f();
	}
	return 0;
}

int
pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
	int i;

	for(i = 0; i < PTHREAD_KEYS_MAX; i++)
		if(!keys[i].used){
			keys[i].used = 1;
			keys[i].dtor = dtor;
			keys[i].v = NULL;
			*k = i;
			return 0;
		}
	return EAGAIN;
}

int
pthread_key_delete(pthread_key_t k)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return EINVAL;
	keys[k].used = 0;
	return 0;
}

void*
pthread_getspecific(pthread_key_t k)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return NULL;
	return keys[k].v;
}

int
pthread_setspecific(pthread_key_t k, const void *v)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return EINVAL;
	keys[k].v = (void*)v;
	return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_DEFAULT; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { return 0; }

int
pthread_mutexattr_settype(pthread_mutexattr_t *a, int t)
{
	if(t != PTHREAD_MUTEX_NORMAL && t != PTHREAD_MUTEX_ERRORCHECK && t != PTHREAD_MUTEX_RECURSIVE)
		return EINVAL;
	a->type = t;
	return 0;
}

int
pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
	m->locked = 0;
	m->type = a != NULL ? a->type : PTHREAD_MUTEX_DEFAULT;
	return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *m)
{
	return m->locked ? EBUSY : 0;
}

/* The one thread holds it, or it is free: a second lock of a lock not recursive would wait forever. */
int
pthread_mutex_lock(pthread_mutex_t *m)
{
	if(m->locked && m->type != PTHREAD_MUTEX_RECURSIVE)
		return EDEADLK;
	m->locked++;
	return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *m)
{
	if(m->locked && m->type != PTHREAD_MUTEX_RECURSIVE)
		return EBUSY;
	m->locked++;
	return 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *m)
{
	if(m->locked == 0)
		return EPERM;
	m->locked--;
	return 0;
}

int pthread_condattr_init(pthread_condattr_t *a) { a->clock = CLOCK_REALTIME; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { return 0; }

int
pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a)
{
	c->waiting = 0;
	return 0;
}

int
pthread_cond_destroy(pthread_cond_t *c)
{
	return c->waiting ? EBUSY : 0;
}

int pthread_cond_signal(pthread_cond_t *c) { return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { return 0; }

int
pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
	int n;

	if(m->locked == 0)
		return EPERM;
	n = m->locked;
	m->locked = 0;
	c->waiting++;
	pause();
	c->waiting--;
	m->locked = n;
	return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t)
{
	struct timespec now, d;
	int n, r;

	if(t->tv_nsec < 0 || t->tv_nsec >= 1000000000)
		return EINVAL;
	if(m->locked == 0)
		return EPERM;
	clock_gettime(CLOCK_REALTIME, &now);
	if(now.tv_sec > t->tv_sec || now.tv_sec == t->tv_sec && now.tv_nsec >= t->tv_nsec)
		return ETIMEDOUT;
	d.tv_sec = t->tv_sec - now.tv_sec;
	d.tv_nsec = t->tv_nsec - now.tv_nsec;
	if(d.tv_nsec < 0){
		d.tv_sec--;
		d.tv_nsec += 1000000000;
	}
	n = m->locked;
	m->locked = 0;
	c->waiting++;
	r = nanosleep(&d, NULL);
	c->waiting--;
	m->locked = n;
	return r < 0 ? 0 : ETIMEDOUT;
}
