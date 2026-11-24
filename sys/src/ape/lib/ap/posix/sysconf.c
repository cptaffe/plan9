#include	<stdio.h>
#include	<unistd.h>
#include	<limits.h>
#include	<time.h>
#include	<errno.h>
#include	<sys/limits.h>
#include	<fcntl.h>
#include	<stdlib.h>
#include	<string.h>

static long
swap(int av)
{
	char b[512], *p, *e;
	long long n, m, mem, pg, used, user;
	int f, r;

	f = open("/dev/swap", O_RDONLY);
	if(f < 0)
		return -1;
	r = read(f, b, sizeof b - 1);
	close(f);
	if(r < 0)
		return -1;
	b[r] = 0;
	mem = pg = used = user = -1;
	for(p = b; *p != 0; p = e+1){
		n = strtoll(p, &e, 10);
		m = -1;
		if(*e == '/')
			m = strtoll(e+1, &e, 10);
		if(strncmp(e, " memory\n", 8) == 0)
			mem = n;
		else if(strncmp(e, " pagesize\n", 10) == 0)
			pg = n;
		else if(strncmp(e, " user\n", 6) == 0){
			used = n;
			user = m;
		}
		if((e = strchr(e, '\n')) == NULL)
			break;
	}
	if(!av && mem >= 0)
		return mem / getpagesize();
	if(av && pg > 0 && user >= 0)
		return used < user ? (user-used)*pg / getpagesize() : 0;
	errno = EINVAL;
	return -1;
}

static long
phys(long n)
{
	long m;

	if(sizeof(void*) < sizeof(long long))
		m = (long)((1ULL<<(8*sizeof(void*))) / getpagesize());
	else
		m = LONG_MAX / getpagesize();
	if(n < 0 || n > m)
		return m;
	return n;
}

long
sysconf(int name)
{
	switch(name)
	{
	case _SC_ARG_MAX:
		return ARG_MAX;
	case _SC_CHILD_MAX:
		return CHILD_MAX;
	case _SC_CLK_TCK:
		return CLOCKS_PER_SEC;
	case _SC_NGROUPS_MAX:
		return NGROUPS_MAX;
	case _SC_OPEN_MAX:
		return OPEN_MAX;
	case _SC_JOB_CONTROL:
#ifdef	_POSIX_JOB_CONTROL
		return _POSIX_JOB_CONTROL;
#else
		return -1;
#endif
	case _SC_SAVED_IDS:
#ifdef	_POSIX_SAVED_IDS
		return _POSIX_SAVED_IDS;
#else
		return -1;
#endif
	case _SC_VERSION:
		return _POSIX_VERSION;
	case _SC_LOGIN_NAME_MAX:
		return L_cuserid;
	case _SC_PAGESIZE:
		return getpagesize();
	case _SC_PHYS_PAGES:
		return phys(swap(0));
	case _SC_AVPHYS_PAGES:
		return phys(swap(1));
	}
	errno = EINVAL;
	return -1;
}
