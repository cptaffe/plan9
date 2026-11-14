#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

pid_t
getpgid(pid_t pid)
{
	int n, f;
	char buf[16], name[32];

	if(pid == 0)
		pid = getpid();
	snprintf(name, sizeof name, "/proc/%d/noteid", pid);
	if((f = open(name, O_RDONLY)) < 0){
		errno = ESRCH;
		return -1;
	}
	n = read(f, buf, sizeof buf - 1);
	close(f);
	if(n <= 0){
		errno = ESRCH;
		return -1;
	}
	buf[n] = 0;
	return atoi(buf);
}
