#include <unistd.h>

extern char **environ;

int
execv(const char *name, char *const argv[])
{
	return execve(name, argv, environ);
}
