#include <unistd.h>
#include <time.h>

int
usleep(unsigned int us)
{
	struct timespec t;

	t.tv_sec = us/1000000;
	t.tv_nsec = (us%1000000)*1000;
	return nanosleep(&t, NULL);
}
