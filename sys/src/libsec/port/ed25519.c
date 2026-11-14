#include "os.h"
#include <libsec.h>
#include "fe25519.h"

/*
 * Ed25519 (RFC 8032): a key is a 32-byte secret, a, and its 32-byte public key, pk; a signature
 * is 64 bytes. Points are extended coordinates (X, Y, Z, T) on -x²+y² = 1+dx²y², on fe25519.c's
 * field; scalars mod L are 32 bytes, little-endian, reduced in constant time as TweetNaCl's
 * modL (public domain) does.
 */

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

static Fe d, d2, sqrtm1;
static Pt base;
static int inited;

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

/* r = p+q, complete for a = -1 (RFC 8032, 5.1.4) */
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

/* r = k·p, k 32 bytes little-endian, by a ladder that does the same for each bit */
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

/* RFC 8032, 5.1.3; -1 if s is no point */
static int
ptdecode(Pt *p, uchar *s)
{
	uchar b[32], c[32];
	Fe u, v, v3, x, t;
	int sign;

	memmove(b, s, 32);
	sign = b[31] >> 7;
	b[31] &= 127;
	fefrombytes(p->y, b);
	fetobytes(c, p->y);
	if(memcmp(b, c, 32) != 0)
		return -1;	/* y ≥ p */
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
	if(feiszero(x) && sign)
		return -1;
	if(feodd(x) != sign)
		feneg(x, x);
	memmove(p->x, x, sizeof(Fe));
	femul(p->t, p->x, p->y);
	return 0;
}

static void
init(void)
{
	uchar s[32];
	Fe t;

	if(inited)
		return;
	feset(t, 121666);
	feinv(t, t);
	feset(d, 121665);
	femul(d, d, t);
	feneg(d, d);
	feadd(d2, d, d);
	feset(t, 2);
	fepow22523(sqrtm1, t);
	fesq(sqrtm1, sqrtm1);
	femul(sqrtm1, sqrtm1, t);	/* 2·2^((p-5)/4) = 2^((p-1)/4), a square root of -1 */
	feset(t, 5);
	feinv(t, t);
	feset(base.y, 4);
	femul(base.y, base.y, t);
	fetobytes(s, base.y);
	ptdecode(&base, s);
	inited = 1;
}

/* the 64-byte hash h of the secret: a (clamped) in the first half, the prefix in the second */
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

/* out = x mod L, x 64 limbs of signed bytes, which this clobbers */
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

/* out = h mod L, h 64 bytes */
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

/* out = (r + k·a) mod L, each 32 bytes */
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

	init();
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

	init();
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

/* 1 if sig is pk's signature of msg, 0 if not */
int
ed25519verify(uchar *sig, uchar *msg, ulong len, uchar *pk)
{
	uchar k[32], x[32], y[32];
	Pt pa, pr, p, q;
	int i;

	init();
	for(i = 31; i >= 0 && sig[32+i] == Lbytes[i]; i--)
		;
	if(i < 0 || sig[32+i] > Lbytes[i])
		return 0;	/* S ≥ L */
	if(ptdecode(&pa, pk) < 0 || ptdecode(&pr, sig) < 0)
		return 0;
	hram(k, sig, pk, msg, len);
	ptmul(&p, sig+32, &base);
	ptmul(&q, k, &pa);
	ptadd(&q, &q, &pr);
	ptencode(x, &p);
	ptencode(y, &q);
	return memcmp(x, y, 32) == 0;
}
