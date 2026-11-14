#include "os.h"
#include "fe25519.h"

static void
squeeze(Fe a)
{
	int j;
	u32int u;

	u = 0;
	for(j = 0; j < 31; j++){
		u += a[j];
		a[j] = u & 255;
		u >>= 8;
	}
	u += a[31];
	a[31] = u & 127;
	u = 19*(u>>7);
	for(j = 0; j < 31; j++){
		u += a[j];
		a[j] = u & 255;
		u >>= 8;
	}
	a[31] += u;
}

static void
add(Fe out, Fe a, Fe b)
{
	int j;
	u32int u;

	u = 0;
	for(j = 0; j < 31; j++){
		u += a[j] + b[j];
		out[j] = u & 255;
		u >>= 8;
	}
	out[31] = u + a[31] + b[31];
}

void
feadd(Fe out, Fe a, Fe b)
{
	add(out, a, b);
	squeeze(out);
}

void
fesub(Fe out, Fe a, Fe b)
{
	int j;
	u32int u;

	u = 218;
	for(j = 0; j < 31; j++){
		u += a[j] + 65280 - b[j];
		out[j] = u & 255;
		u >>= 8;
	}
	out[31] = u + a[31] - b[31];
	squeeze(out);
}

void
femul(Fe out, Fe a, Fe b)
{
	int i, j;
	u32int u;
	Fe t;

	for(i = 0; i < 32; i++){
		u = 0;
		for(j = 0; j <= i; j++)
			u += a[j]*b[i-j];
		for(j = i+1; j < 32; j++)
			u += 38*a[j]*b[i+32-j];
		t[i] = u;
	}
	squeeze(t);
	memmove(out, t, sizeof t);
}

void
fesq(Fe out, Fe a)
{
	femul(out, a, a);
}

void
femul121665(Fe out, Fe a)
{
	int j;
	u32int u;

	u = 0;
	for(j = 0; j < 31; j++){
		u += 121665*a[j];
		out[j] = u & 255;
		u >>= 8;
	}
	u += 121665*a[31];
	out[31] = u & 127;
	u = 19*(u>>7);
	for(j = 0; j < 31; j++){
		u += out[j];
		out[j] = u & 255;
		u >>= 8;
	}
	out[31] += u;
}

static Fe minusp = {
	19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128,
};

void
fefreeze(Fe a)
{
	int j;
	u32int neg;
	Fe o;

	squeeze(a);
	memmove(o, a, sizeof o);
	add(a, a, minusp);
	neg = -((a[31]>>7) & 1);
	for(j = 0; j < 32; j++)
		a[j] ^= neg & (o[j] ^ a[j]);
}

static void
nsq(Fe out, Fe a, int n)
{
	fesq(out, a);
	while(--n > 0)
		fesq(out, out);
}

/* z^(2^250-1) in t, and z^11 in z11, as the reference's recip */
static void
pow250(Fe t, Fe z11, Fe z)
{
	Fe z2, z9, a, b, c, d;

	fesq(z2, z);
	nsq(a, z2, 2);
	femul(z9, a, z);
	femul(z11, z9, z2);
	fesq(a, z11);
	femul(a, a, z9);	/* 2^5-1 */
	nsq(b, a, 5);
	femul(b, b, a);		/* 2^10-1 */
	nsq(c, b, 10);
	femul(c, c, b);		/* 2^20-1 */
	nsq(d, c, 20);
	femul(d, d, c);		/* 2^40-1 */
	nsq(d, d, 10);
	femul(d, d, b);		/* 2^50-1 */
	nsq(c, d, 50);
	femul(c, c, d);		/* 2^100-1 */
	nsq(a, c, 100);
	femul(a, a, c);		/* 2^200-1 */
	nsq(a, a, 50);
	femul(t, a, d);		/* 2^250-1 */
}

/* z^(p-2) = z^(2^255-21) */
void
feinv(Fe out, Fe z)
{
	Fe t, z11;

	pow250(t, z11, z);
	nsq(t, t, 5);
	femul(out, t, z11);
}

/* z^((p-5)/8) = z^(2^252-3) */
void
fepow22523(Fe out, Fe z)
{
	Fe t, z11;

	pow250(t, z11, z);
	nsq(t, t, 2);
	femul(out, t, z);
}

void
fecswap(Fe a, Fe b, u32int bit)
{
	int j;
	u32int m, t;

	m = -bit;
	for(j = 0; j < 32; j++){
		t = m & (a[j] ^ b[j]);
		a[j] ^= t;
		b[j] ^= t;
	}
}

void
fefrombytes(Fe a, uchar *s)
{
	int j;

	for(j = 0; j < 32; j++)
		a[j] = s[j];
	squeeze(a);
}

void
fetobytes(uchar *s, Fe a)
{
	int j;
	Fe t;

	memmove(t, a, sizeof t);
	fefreeze(t);
	for(j = 0; j < 32; j++)
		s[j] = t[j];
}

void
feset(Fe a, u32int v)
{
	memset(a, 0, sizeof(Fe));
	a[0] = v & 255;
	a[1] = (v>>8) & 255;
	a[2] = (v>>16) & 255;
	a[3] = v>>24;
}
