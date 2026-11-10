#include <sys/types.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static void
tv(struct timeval *t, clock_t c)
{
	t->tv_sec = c / CLOCKS_PER_SEC;
	t->tv_usec = c % CLOCKS_PER_SEC * (1000000 / CLOCKS_PER_SEC);
}

/* the times are times()'s, /dev/cputime's; Plan 9 counts nothing else */
int
getrusage(int who, struct rusage *ru)
{
	struct tms t;

	if(who != RUSAGE_SELF && who != RUSAGE_CHILDREN){
		errno = EINVAL;
		return -1;
	}
	times(&t);
	memset(ru, 0, sizeof *ru);
	if(who == RUSAGE_SELF){
		tv(&ru->ru_utime, t.tms_utime);
		tv(&ru->ru_stime, t.tms_stime);
	}else{
		tv(&ru->ru_utime, t.tms_cutime);
		tv(&ru->ru_stime, t.tms_cstime);
	}
	return 0;
}
