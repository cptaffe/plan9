#ifndef __THREADS_H
#define __THREADS_H
#pragma lib "/$M/lib/ape/libap.a"

#include <time.h>
#include <pthread.h>

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define thread_local	_Thread_local
#endif

#define ONCE_FLAG_INIT	PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS	PTHREAD_DESTRUCTOR_ITERATIONS

typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;
typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

enum {
	thrd_success,
	thrd_busy,
	thrd_error,
	thrd_nomem,
	thrd_timedout,
};

enum {
	mtx_plain = 0,
	mtx_recursive = 1,
	mtx_timed = 2,
};

#ifdef __cplusplus
extern "C" {
#endif

extern void call_once(once_flag *, void (*)(void));

extern int cnd_broadcast(cnd_t *);
extern void cnd_destroy(cnd_t *);
extern int cnd_init(cnd_t *);
extern int cnd_signal(cnd_t *);
extern int cnd_timedwait(cnd_t *, mtx_t *, const struct timespec *);
extern int cnd_wait(cnd_t *, mtx_t *);

extern void mtx_destroy(mtx_t *);
extern int mtx_init(mtx_t *, int);
extern int mtx_lock(mtx_t *);
extern int mtx_timedlock(mtx_t *, const struct timespec *);
extern int mtx_trylock(mtx_t *);
extern int mtx_unlock(mtx_t *);

extern int thrd_create(thrd_t *, thrd_start_t, void *);
extern thrd_t thrd_current(void);
extern int thrd_detach(thrd_t);
extern int thrd_equal(thrd_t, thrd_t);
extern _Noreturn void thrd_exit(int);
extern int thrd_join(thrd_t, int *);
extern int thrd_sleep(const struct timespec *, struct timespec *);
extern void thrd_yield(void);

extern int tss_create(tss_t *, tss_dtor_t);
extern void tss_delete(tss_t);
extern void *tss_get(tss_t);
extern int tss_set(tss_t, void *);

#ifdef __cplusplus
}
#endif

#endif
