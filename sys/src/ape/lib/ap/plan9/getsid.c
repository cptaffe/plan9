#include <sys/types.h>
#include <unistd.h>

pid_t
getsid(pid_t pid)
{
	return getpgid(pid);
}
