#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

void *
reallocarray(void *p, size_t n, size_t m)
{
	if(m != 0 && n > SIZE_MAX/m){
		errno = ENOMEM;
		return NULL;
	}
	return realloc(p, n*m);
}
