#include <unistd.h>

/* wasm's page: memory grows by it */
int
getpagesize(void)
{
	return 65536;
}
