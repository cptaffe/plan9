#include <threads.h>
#include <errno.h>
#include <sched.h>
#include <time.h>

static int
ret(int e)
{
	switch(e){
	case 0:
		return thrd_success;
	case EBUSY:
		return thrd_busy;
	case ETIMEDOUT:
		return thrd_timedout;
	case ENOMEM:
		return thrd_nomem;
	}
	return thrd_error;
}

void
call_once(once_flag *o, void (*f)(void))
{
	pthread_once(o, f);
}

int cnd_broadcast(cnd_t *c) { return ret(pthread_cond_broadcast(c)); }
void cnd_destroy(cnd_t *c) { pthread_cond_destroy(c); }
int cnd_init(cnd_t *c) { return ret(pthread_cond_init(c, NULL)); }
int cnd_signal(cnd_t *c) { return ret(pthread_cond_signal(c)); }
int cnd_timedwait(cnd_t *c, mtx_t *m, const struct timespec *t) { return ret(pthread_cond_timedwait(c, m, t)); }
int cnd_wait(cnd_t *c, mtx_t *m) { return ret(pthread_cond_wait(c, m)); }

void mtx_destroy(mtx_t *m) { pthread_mutex_destroy(m); }

int
mtx_init(mtx_t *m, int type)
{
	pthread_mutexattr_t a;

	if((type & ~(mtx_recursive|mtx_timed)) != 0)
		return thrd_error;
	pthread_mutexattr_init(&a);
	if(type & mtx_recursive)
		pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	return ret(pthread_mutex_init(m, &a));
}

int mtx_lock(mtx_t *m) { return ret(pthread_mutex_lock(m)); }
int mtx_trylock(mtx_t *m) { return ret(pthread_mutex_trylock(m)); }
int mtx_unlock(mtx_t *m) { return ret(pthread_mutex_unlock(m)); }

int
mtx_timedlock(mtx_t *m, const struct timespec *t)
{
	pthread_cond_t c = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t w = PTHREAD_MUTEX_INITIALIZER;
	int e;

	if(pthread_mutex_trylock(m) == 0)
		return thrd_success;
	pthread_mutex_lock(&w);
	while((e = pthread_cond_timedwait(&c, &w, t)) == 0)
		if(pthread_mutex_trylock(m) == 0)
			return thrd_success;
	return ret(e);
}

int
thrd_create(thrd_t *t, thrd_start_t f, void *arg)
{
	return thrd_error;
}

thrd_t thrd_current(void) { return pthread_self(); }
int thrd_detach(thrd_t t) { return ret(pthread_detach(t)); }
int thrd_equal(thrd_t a, thrd_t b) { return pthread_equal(a, b); }

void
thrd_exit(int r)
{
	pthread_exit((void*)(long)r);
}

int
thrd_join(thrd_t t, int *r)
{
	return ret(pthread_join(t, NULL));
}

int
thrd_sleep(const struct timespec *t, struct timespec *rem)
{
	if(nanosleep(t, rem) == 0)
		return 0;
	return errno == EINTR ? -1 : -2;
}

void thrd_yield(void) { sched_yield(); }

int tss_create(tss_t *k, tss_dtor_t d) { return ret(pthread_key_create(k, d)); }
void tss_delete(tss_t k) { pthread_key_delete(k); }
void *tss_get(tss_t k) { return pthread_getspecific(k); }
int tss_set(tss_t k, void *v) { return ret(pthread_setspecific(k, v)); }
