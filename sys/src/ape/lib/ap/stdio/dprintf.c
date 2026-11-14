#include "iolib.h"
#include <stdarg.h>
#include <unistd.h>

int
vdprintf(int fd, const char *fmt, va_list ap)
{
	char buf[1024], *p;
	va_list aq;
	int n, w, t;

	va_copy(aq, ap);
	n = vsnprintf(buf, sizeof buf, fmt, aq);
	va_end(aq);
	if(n < 0)
		return -1;
	p = buf;
	if(n >= sizeof buf){
		if((p = malloc(n+1)) == NULL)
			return -1;
		vsnprintf(p, n+1, fmt, ap);
	}
	for(t = 0; t < n; t += w)
		if((w = write(fd, p+t, n-t)) <= 0){
			n = -1;
			break;
		}
	if(p != buf)
		free(p);
	return n;
}

int
dprintf(int fd, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vdprintf(fd, fmt, ap);
	va_end(ap);
	return n;
}
