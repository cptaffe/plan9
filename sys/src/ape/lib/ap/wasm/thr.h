enum {
	Thrstack = 512*1024,
	Thrabuf = 256*1024,
	Nsig = 30,
};

typedef struct Waiter Waiter;
struct Waiter {
	int fiber;
	int woken;
	int timed;
	Waiter *next;
};

typedef struct Thr Thr;
struct Thr {
	pthread_t id;
	int fiber;
	int pid;
	int done;
	int detached;
	void *ret;
	void *(*f)(void *);
	void *arg;
	char *stk;
	char *abuf;
	char *tls;
	Waiter *joinq;
	void *keys[PTHREAD_KEYS_MAX];
};

extern Thr _thrmain;
extern Thr **_thr, *_thrdead;
extern int _nthr, _nalive;

extern Thr *_thrself(void);
extern Thr *_thrget(pthread_t);
extern void _thrwait(Waiter **, Waiter *);
extern int _thrwake(Waiter **);
extern int _thrsignal(pthread_t, int);
extern int (*_thrkill)(pthread_t, int);
extern void _thrfree(Thr *);
extern int _thrlockfile(FILE *, int);
extern void _thrunlockfile(FILE *);
extern int (*_lockfile)(FILE *, int);
extern void (*_unlockfile)(FILE *);
extern int (*_thrdie)(char*);
extern pid_t _grouppid;
extern pid_t _getpid(void);
