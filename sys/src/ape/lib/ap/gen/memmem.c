#include <string.h>

void *
memmem(const void *h, size_t hn, const void *n, size_t nn)
{
	const char *p, *e;

	if(nn == 0)
		return (void*)h;
	if(hn < nn)
		return NULL;
	e = (const char*)h + hn - nn;
	for(p = h; p <= e; p++){
		if((p = memchr(p, *(const char*)n, e-p+1)) == NULL)
			return NULL;
		if(memcmp(p, n, nn) == 0)
			return (void*)p;
	}
	return NULL;
}
