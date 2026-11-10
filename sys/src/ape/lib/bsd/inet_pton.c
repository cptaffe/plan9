/* posix */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

/* ddd.ddd.ddd.ddd, each at most 255, into 4 bytes */
static int
v4(const char *s, unsigned char *a)
{
	int i, n, d;

	for(i = 0; i < 4; i++){
		if(i > 0 && *s++ != '.')
			return 0;
		if(*s < '0' || *s > '9')
			return 0;
		n = 0;
		for(d = 0; *s >= '0' && *s <= '9'; d++){
			if(d > 0 && n == 0)
				return 0;	/* a leading zero */
			n = n*10 + *s++ - '0';
			if(n > 255)
				return 0;
		}
		a[i] = n;
	}
	return *s == 0;
}

static int
hex(int c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* RFC 4291's text: eight groups of hex, :: once for zeros, a v4 address last */
static int
v6(const char *s, unsigned char *a)
{
	unsigned char b[16];
	int n, i, x, d, gap;
	const char *g;

	memset(b, 0, sizeof b);
	n = 0;
	gap = -1;
	if(*s == ':'){
		if(*++s != ':')
			return 0;
	}
	while(*s){
		if(*s == ':'){
			if(gap >= 0)
				return 0;
			gap = n;
			s++;
			if(*s == 0)
				break;
			continue;
		}
		if(n == 16)
			return 0;
		g = s;
		x = 0;
		for(d = 0; d < 4 && hex(*s) >= 0; d++)
			x = x<<4 | hex(*s++);
		if(*s == '.'){	/* the last 32 bits, as v4 */
			if(n > 12 || !v4(g, b+n))
				return 0;
			n += 4;
			break;
		}
		if(d == 0)
			return 0;
		b[n++] = x>>8;
		b[n++] = x;
		if(*s == 0)
			break;
		if(*s++ != ':' || *s == 0)
			return 0;
	}
	if(gap >= 0){
		if(n == 16)
			return 0;
		i = 16 - (n - gap);
		memmove(b+i, b+gap, n-gap);
		memset(b+gap, 0, i-gap);
	}else if(n != 16)
		return 0;
	memcpy(a, b, 16);
	return 1;
}

int
inet_pton(int af, const char *src, void *dst)
{
	switch(af){
	case AF_INET:
		return v4(src, dst);
	case AF_INET6:
		return v6(src, dst);
	}
	errno = EAFNOSUPPORT;
	return -1;
}
