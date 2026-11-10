#include <math.h>
#include <errno.h>

/*
 * x*y + z, rounded once.  Each argument is m*2^e, m an integer of
 * 53 bits; the product of the m's is exact in 128 bits, where the
 * sum is made, with the bits that fall off kept as a sticky bit.
 */
typedef unsigned long long uvlong;
typedef struct { uvlong hi, lo; } W;

static void
unpack(double x, int *s, uvlong *m, int *e)
{
	union { double d; uvlong u; } a;
	int be;

	a.d = x;
	*s = a.u >> 63;
	be = (a.u >> 52) & 0x7ff;
	*m = a.u & ((1ULL<<52)-1);
	if(be == 0)
		*e = -1074;
	else{
		*m |= 1ULL<<52;
		*e = be - 1075;
	}
}

static W
mul(uvlong a, uvlong b)
{
	uvlong al, ah, bl, bh, m1, m2, lo;
	W r;

	al = a & 0xffffffff;
	ah = a >> 32;
	bl = b & 0xffffffff;
	bh = b >> 32;
	lo = al*bl;
	m1 = ah*bl;
	m2 = al*bh;
	r.hi = ah*bh + (m1>>32) + (m2>>32);
	r.lo = lo + (m1<<32);
	if(r.lo < lo)
		r.hi++;
	lo = r.lo;
	r.lo += m2<<32;
	if(r.lo < lo)
		r.hi++;
	return r;
}

static int
top(W a)	/* the index of the leading bit; a is not 0 */
{
	int n;
	uvlong v;

	n = a.hi? 64: 0;
	v = a.hi? a.hi: a.lo;
	while(v >>= 1)
		n++;
	return n;
}

static W
shl(W a, int n)
{
	if(n >= 64){
		a.hi = a.lo << (n-64);
		a.lo = 0;
	}else if(n > 0){
		a.hi = a.hi<<n | a.lo>>(64-n);
		a.lo <<= n;
	}
	return a;
}

static int
below(W a, int n)	/* whether a bit under n is set */
{
	if(n <= 0)
		return 0;
	if(n >= 128)
		return a.hi || a.lo;
	if(n >= 64)
		return a.lo || (n > 64 && (a.hi & ((1ULL<<(n-64))-1)));
	return (a.lo & ((1ULL<<n)-1)) != 0;
}

static int
bit(W a, int n)
{
	return n >= 64? (a.hi >> (n-64)) & 1: (a.lo >> n) & 1;
}

static W
shr(W a, int n)	/* the bits shifted out are a sticky bit 0 */
{
	uvlong st;

	if(n <= 0)
		return a;
	st = below(a, n);
	if(n >= 128)
		a.hi = a.lo = 0;
	else if(n >= 64){
		a.lo = a.hi >> (n-64);
		a.hi = 0;
	}else{
		a.lo = a.lo>>n | a.hi<<(64-n);
		a.hi >>= n;
	}
	a.lo |= st;
	return a;
}

static int
less(W a, W b)
{
	return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}

double
fma(double x, double y, double z)
{
	int sx, sy, sz, sa, sb, ex, ey, ez, ea, eb, l, L, sh;
	uvlong mx, my, mz, m, half, rest;
	W a, b, t;
	double r;

	if(!isfinite(x) || !isfinite(y))
		return x*y + z;
	if(!isfinite(z))
		return z;
	if(x == 0 || y == 0)
		return x*y + z;	/* the product is exact */
	if(z == 0)
		return x*y;
	unpack(x, &sx, &mx, &ex);
	unpack(y, &sy, &my, &ey);
	unpack(z, &sz, &mz, &ez);

	/* the product and z, each with its leading bit at 125 */
	a = mul(mx, my);
	sa = sx ^ sy;
	ea = ex + ey;
	sh = 125 - top(a);
	a = shl(a, sh);
	ea -= sh;
	b.hi = 0;
	b.lo = mz;
	sb = sz;
	eb = ez;
	sh = 125 - top(b);
	b = shl(b, sh);
	eb -= sh;

	/* a, the larger */
	if(eb > ea || (eb == ea && less(a, b))){
		t = a, a = b, b = t;
		sh = sa, sa = sb, sb = sh;
		sh = ea, ea = eb, eb = sh;
	}
	b = shr(b, ea - eb);
	if(sa == sb){
		a.lo += b.lo;
		a.hi += b.hi + (a.lo < b.lo);
	}else{
		a.hi -= b.hi + (a.lo < b.lo);
		a.lo -= b.lo;
	}
	if(a.hi == 0 && a.lo == 0)
		return 0;	/* exactly: +0, as rounding to nearest has it */

	/* round to 53 bits, or fewer if subnormal */
	l = top(a);
	L = l + ea;
	if(L >= 1024){
		errno = ERANGE;
		return sa? -HUGE_VAL: HUGE_VAL;
	}
	sh = l - 52;
	if(L < -1022)
		sh += -1022 - L;
	if(sh <= 0)
		m = a.lo << -sh;
	else if(sh > 127)
		m = 0;	/* below half the least subnormal */
	else{
		half = bit(a, sh-1);
		rest = below(a, sh-1);
		m = sh >= 64? a.hi >> (sh-64): (a.lo>>sh | a.hi<<(64-sh));
		if(half && (rest || (m & 1)))
			m++;
	}
	r = ldexp((double)m, ea + sh);
	return sa? -r: r;
}
