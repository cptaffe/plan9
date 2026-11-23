#include "os.h"
#include <libsec.h>
#include "fe25519.h"

typedef struct Pt Pt;
struct Pt {
	Fe	x;
	Fe	y;
	Fe	z;
	Fe	t;
};

static vlong Lbytes[32] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10,
};

static Fe d = {
	0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75, 0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70, 0x00,
	0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c, 0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c, 0x03, 0x52,
};

static Fe d2 = {
	0x59, 0xf1, 0xb2, 0x26, 0x94, 0x9b, 0xd6, 0xeb, 0x56, 0xb1, 0x83, 0x82, 0x9a, 0x14, 0xe0, 0x00,
	0x30, 0xd1, 0xf3, 0xee, 0xf2, 0x80, 0x8e, 0x19, 0xe7, 0xfc, 0xdf, 0x56, 0xdc, 0xd9, 0x06, 0x24,
};

static Fe sqrtm1 = {
	0xb0, 0xa0, 0x0e, 0x4a, 0x27, 0x1b, 0xee, 0xc4, 0x78, 0xe4, 0x2f, 0xad, 0x06, 0x18, 0x43, 0x2f,
	0xa7, 0xd7, 0xfb, 0x3d, 0x99, 0x00, 0x4d, 0x2b, 0x0b, 0xdf, 0xc1, 0x4f, 0x80, 0x24, 0x83, 0x2b,
};

static Pt base = {
	{
		0x1a, 0xd5, 0x25, 0x8f, 0x60, 0x2d, 0x56, 0xc9, 0xb2, 0xa7, 0x25, 0x95, 0x60, 0xc7, 0x2c, 0x69,
		0x5c, 0xdc, 0xd6, 0xfd, 0x31, 0xe2, 0xa4, 0xc0, 0xfe, 0x53, 0x6e, 0xcd, 0xd3, 0x36, 0x69, 0x21,
	},
	{
		0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
		0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	},
	{1},
	{
		0xa3, 0xdd, 0xb7, 0xa5, 0xb3, 0x8a, 0xde, 0x6d, 0xf5, 0x52, 0x51, 0x77, 0x80, 0x9f, 0xf0, 0x20,
		0x7d, 0xe3, 0xab, 0x64, 0x8e, 0x4e, 0xea, 0x66, 0x65, 0x76, 0x8b, 0xd7, 0x0f, 0x5f, 0x87, 0x67,
	},
};

static void
feneg(Fe out, Fe a)
{
	Fe z;

	feset(z, 0);
	fesub(out, z, a);
}

static int
feiszero(Fe a)
{
	uchar s[32];
	int i, c;

	fetobytes(s, a);
	c = 0;
	for(i = 0; i < 32; i++)
		c |= s[i];
	return c == 0;
}

static int
feodd(Fe a)
{
	uchar s[32];

	fetobytes(s, a);
	return s[0] & 1;
}

static void
ptzero(Pt *p)
{
	feset(p->x, 0);
	feset(p->y, 1);
	feset(p->z, 1);
	feset(p->t, 0);
}

static void
ptadd(Pt *r, Pt *p, Pt *q)
{
	Fe a, b, c, dd, e, f, g, h, t;

	fesub(a, p->y, p->x);
	fesub(t, q->y, q->x);
	femul(a, a, t);
	feadd(b, p->y, p->x);
	feadd(t, q->y, q->x);
	femul(b, b, t);
	femul(c, p->t, q->t);
	femul(c, c, d2);
	femul(dd, p->z, q->z);
	feadd(dd, dd, dd);
	fesub(e, b, a);
	fesub(f, dd, c);
	feadd(g, dd, c);
	feadd(h, b, a);
	femul(r->x, e, f);
	femul(r->y, g, h);
	femul(r->t, e, h);
	femul(r->z, f, g);
}

static void
ptcswap(Pt *p, Pt *q, u32int bit)
{
	fecswap(p->x, q->x, bit);
	fecswap(p->y, q->y, bit);
	fecswap(p->z, q->z, bit);
	fecswap(p->t, q->t, bit);
}

static void
ptmul(Pt *r, uchar *k, Pt *p)
{
	Pt r0, r1;
	u32int bit;
	int i;

	ptzero(&r0);
	r1 = *p;
	for(i = 255; i >= 0; i--){
		bit = (k[i/8] >> (i&7)) & 1;
		ptcswap(&r0, &r1, bit);
		ptadd(&r1, &r0, &r1);
		ptadd(&r0, &r0, &r0);
		ptcswap(&r0, &r1, bit);
	}
	*r = r0;
	memset(&r0, 0, sizeof r0);
	memset(&r1, 0, sizeof r1);
}

static void
ptencode(uchar *s, Pt *p)
{
	Fe zi, x, y;

	feinv(zi, p->z);
	femul(x, p->x, zi);
	femul(y, p->y, zi);
	fetobytes(s, y);
	s[31] |= feodd(x) << 7;
}

static int
ptdecode(Pt *p, uchar *s)
{
	uchar b[32];
	Fe u, v, v3, x, t;
	int sign;

	memmove(b, s, 32);
	sign = b[31] >> 7;
	b[31] &= 127;
	fefrombytes(p->y, b);
	feset(p->z, 1);
	fesq(u, p->y);
	femul(v, u, d);
	fesub(u, u, p->z);
	feadd(v, v, p->z);
	fesq(v3, v);
	femul(v3, v3, v);
	fesq(x, v3);
	femul(x, x, v);
	femul(x, x, u);
	fepow22523(x, x);
	femul(x, x, v3);
	femul(x, x, u);
	fesq(t, x);
	femul(t, t, v);
	fesub(t, t, u);
	if(!feiszero(t)){
		feadd(t, t, u);
		feadd(t, t, u);
		if(!feiszero(t))
			return -1;
		femul(x, x, sqrtm1);
	}
	if(feodd(x) != sign)
		feneg(x, x);
	memmove(p->x, x, sizeof(Fe));
	femul(p->t, p->x, p->y);
	return 0;
}

static void
expand(uchar h[64], uchar *a)
{
	DigestState s;

	memset(&s, 0, sizeof s);
	sha2_512(a, 32, h, &s);
	memset(&s, 0, sizeof s);
	h[0] &= 248;
	h[31] &= 127;
	h[31] |= 64;
}

static void
modl(uchar *out, vlong x[64])
{
	vlong carry;
	int i, j;

	for(i = 63; i >= 32; i--){
		carry = 0;
		for(j = i-32; j < i-12; j++){
			x[j] += carry - 16*x[i]*Lbytes[j-(i-32)];
			carry = (x[j]+128) >> 8;
			x[j] -= carry << 8;
		}
		x[j] += carry;
		x[i] = 0;
	}
	carry = 0;
	for(j = 0; j < 32; j++){
		x[j] += carry - (x[31]>>4)*Lbytes[j];
		carry = x[j] >> 8;
		x[j] &= 255;
	}
	for(j = 0; j < 32; j++)
		x[j] -= carry*Lbytes[j];
	for(i = 0; i < 32; i++){
		x[i+1] += x[i] >> 8;
		out[i] = x[i] & 255;
	}
}

static void
reduce(uchar *out, uchar *h)
{
	vlong x[64];
	int i;

	for(i = 0; i < 64; i++)
		x[i] = h[i];
	modl(out, x);
	memset(x, 0, sizeof x);
}

static void
muladd(uchar *out, uchar *r, uchar *k, uchar *a)
{
	vlong x[64];
	int i, j;

	for(i = 0; i < 64; i++)
		x[i] = i < 32 ? r[i] : 0;
	for(i = 0; i < 32; i++)
		for(j = 0; j < 32; j++)
			x[i+j] += (vlong)k[i]*a[j];
	modl(out, x);
	memset(x, 0, sizeof x);
}

void
ed25519pubkey(uchar *pk, uchar *a)
{
	uchar h[64];
	Pt p;

	expand(h, a);
	ptmul(&p, h, &base);
	ptencode(pk, &p);
	memset(h, 0, sizeof h);
	memset(&p, 0, sizeof p);
}

static void
hram(uchar k[32], uchar *r, uchar *pk, uchar *msg, ulong len)
{
	DigestState *s;
	uchar h[64];

	s = sha2_512(r, 32, nil, nil);
	s = sha2_512(pk, 32, nil, s);
	sha2_512(msg, len, h, s);
	reduce(k, h);
}

void
ed25519sign(uchar *sig, uchar *msg, ulong len, uchar *a, uchar *pk)
{
	DigestState s;
	uchar h[64], rh[64], r[32], k[32];
	Pt p;

	expand(h, a);
	memset(&s, 0, sizeof s);
	sha2_512(h+32, 32, nil, &s);
	sha2_512(msg, len, rh, &s);
	memset(&s, 0, sizeof s);
	reduce(r, rh);
	ptmul(&p, r, &base);
	ptencode(sig, &p);
	hram(k, sig, pk, msg, len);
	muladd(sig+32, r, k, h);
	memset(h, 0, sizeof h);
	memset(rh, 0, sizeof rh);
	memset(r, 0, sizeof r);
	memset(&p, 0, sizeof p);
}

int
ed25519verify(uchar *sig, uchar *msg, ulong len, uchar *pk)
{
	uchar k[32], r[32];
	Pt a, p, q;
	int i;

	for(i = 31; i >= 0 && sig[32+i] == Lbytes[i]; i--)
		;
	if(i < 0 || sig[32+i] > Lbytes[i])
		return 0;
	if(ptdecode(&a, pk) < 0)
		return 0;
	feneg(a.x, a.x);
	feneg(a.t, a.t);
	hram(k, sig, pk, msg, len);
	ptmul(&p, sig+32, &base);
	ptmul(&q, k, &a);
	ptadd(&p, &p, &q);
	ptencode(r, &p);
	return memcmp(r, sig, 32) == 0;
}
