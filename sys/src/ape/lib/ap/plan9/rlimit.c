#include <sys/resource.h>
#include <errno.h>

/* Plan 9 limits none of them: see <sys/resource.h> */
int
getrlimit(int resource, struct rlimit *rl)
{
	if(resource < 0 || resource >= RLIM_NLIMITS){
		errno = EINVAL;
		return -1;
	}
	rl->rlim_cur = RLIM_INFINITY;
	rl->rlim_max = RLIM_INFINITY;
	return 0;
}

int
setrlimit(int resource, const struct rlimit *rl)
{
	if(resource < 0 || resource >= RLIM_NLIMITS || rl->rlim_cur > rl->rlim_max){
		errno = EINVAL;
		return -1;
	}
	if(resource == RLIMIT_CORE)
		return 0;	/* there are no core files to limit */
	if(rl->rlim_cur != RLIM_INFINITY || rl->rlim_max != RLIM_INFINITY){
		errno = EINVAL;	/* a limit Plan 9 cannot keep */
		return -1;
	}
	return 0;
}
