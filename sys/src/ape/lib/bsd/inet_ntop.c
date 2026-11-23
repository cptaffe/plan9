/* posix */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const char*
put(const char *s, char *dst, socklen_t size)
{
	if(strlen(s) >= size){
		errno = ENOSPC;
		return NULL;
	}
	strcpy(dst, s);
	return dst;
}

/* RFC 5952's: lower case, the longest run of two zero groups or more as :: */
static const char*
v6(const unsigned char *a, char *dst, socklen_t size)
{
	char buf[INET6_ADDRSTRLEN], *p;
	int g[8], i, best, bestn, n;

	for(i = 0; i < 8; i++)
		g[i] = a[2*i]<<8 | a[2*i+1];
	best = -1;
	bestn = 1;
	for(i = 0; i < 8; i += n ? n : 1){
		for(n = 0; i+n < 8 && g[i+n] == 0; n++)
			;
		if(n > bestn){
			best = i;
			bestn = n;
		}
	}
	p = buf;
	if(best == 0 && (bestn == 6 || (bestn == 5 && g[5] == 0xffff))){
		/* ::a.b.c.d and ::ffff:a.b.c.d */
		sprintf(buf, "::%s%d.%d.%d.%d", bestn == 5 ? "ffff:" : "", a[12], a[13], a[14], a[15]);
		return put(buf, dst, size);
	}
	for(i = 0; i < 8; i++){
		if(i == best){
			*p++ = ':';
			if(i == 0)
				*p++ = ':';
			i += bestn - 1;
			continue;
		}
		p += sprintf(p, "%x", g[i]);
		if(i < 7)
			*p++ = ':';
	}
	*p = 0;
	return put(buf, dst, size);
}

const char*
inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	const unsigned char *a;
	char buf[INET_ADDRSTRLEN];

	a = src;
	switch(af){
	case AF_INET:
		sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
		return put(buf, dst, size);
	case AF_INET6:
		return v6(a, dst, size);
	}
	errno = EAFNOSUPPORT;
	return NULL;
}
