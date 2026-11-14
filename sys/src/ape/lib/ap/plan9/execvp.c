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
	const char *p, *e, **av;
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
			switch(errno){
			case EACCES:
			case EPERM:
				denied = 1;
			case ENOENT:
			case ENOTDIR:
			case ENAMETOOLONG:
				break;
			case ENOEXEC:
				for(n = 0; argv[n]; n++)
					;
				if((av = malloc((n+2) * sizeof *av)) == NULL)
					return -1;
				av[0] = "sh";
				av[1] = buf;
				memcpy(av+2, argv+1, n * sizeof *av);
				execve("/bin/sh", av, environ);
				free(av);
				return -1;
			default:
				return -1;
			}
		}
		if(e == NULL)
			break;
		p = e+1;
	}
	errno = denied ? EACCES : ENOENT;
	return -1;
}
