#include <stdlib.h>

typedef unsigned long long uvlong;

static unsigned short x[3] = {0x330e, 0xabcd, 0x1234};
static unsigned short a[3] = {0xe66d, 0xdeec, 0x0005};
static unsigned short c = 0xb;

static uvlong
step(unsigned short *s)
{
	uvlong v, m;

	v = (uvlong)s[2]<<32 | (uvlong)s[1]<<16 | s[0];
	m = (uvlong)a[2]<<32 | (uvlong)a[1]<<16 | a[0];
	v = (v*m + c) & 0xffffffffffffULL;
	s[0] = v;
	s[1] = v>>16;
	s[2] = v>>32;
	return v;
}

double
erand48(unsigned short s[3])
{
	return (double)step(s) / 281474976710656.0;
}

double
drand48(void)
{
	return erand48(x);
}

long
nrand48(unsigned short s[3])
{
	return step(s) >> 17;
}

long
lrand48(void)
{
	return nrand48(x);
}

long
jrand48(unsigned short s[3])
{
	return (int)(step(s) >> 16);
}

long
mrand48(void)
{
	return jrand48(x);
}

void
srand48(long seed)
{
	x[0] = 0x330e;
	x[1] = seed;
	x[2] = seed>>16;
	a[0] = 0xe66d;
	a[1] = 0xdeec;
	a[2] = 0x0005;
	c = 0xb;
}

unsigned short*
seed48(unsigned short s[3])
{
	static unsigned short old[3];

	old[0] = x[0];
	old[1] = x[1];
	old[2] = x[2];
	x[0] = s[0];
	x[1] = s[1];
	x[2] = s[2];
	a[0] = 0xe66d;
	a[1] = 0xdeec;
	a[2] = 0x0005;
	c = 0xb;
	return old;
}

void
lcong48(unsigned short p[7])
{
	x[0] = p[0];
	x[1] = p[1];
	x[2] = p[2];
	a[0] = p[3];
	a[1] = p[4];
	a[2] = p[5];
	c = p[6];
}
