#include <unistd.h>
#include <sys/limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

extern char **environ;

int
execvp(const char *name, const char **argv)
{
	char buf[PATH_MAX];
	const char *p, *e;
	int n, denied;

	if(*name == 0){
		errno = ENOENT;
		return -1;
	}
	if(strchr(name, '/'))
		return execve(name, argv, environ);
	if((p = getenv("PATH")) == NULL || *p == 0)
		p = "/bin";
	denied = 0;
	for(;;){
		e = strchr(p, ':');
		n = e ? e - p : strlen(p);
		if(snprintf(buf, sizeof buf, "%.*s/%s", n ? n : 1, n ? p : ".", name) < sizeof buf){
			execve(buf, argv, environ);
			if(errno == EACCES)
				denied = 1;
		}
		if(e == NULL)
			break;
		p = e+1;
	}
	errno = denied ? EACCES : ENOENT;
	return -1;
}
