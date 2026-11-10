/*
 * The compiler's call for a product of 128-bit integers (clang
 * makes one of an overflow test of 64-bit ones): the low 128 bits,
 * from 32-bit pieces.
 */
typedef unsigned long long uvlong;
typedef __int128 vvlong;

static void
mul64(uvlong a, uvlong b, uvlong *hi, uvlong *lo)
{
	uvlong al, ah, bl, bh, m1, m2, t;

	al = a & 0xffffffff;
	ah = a >> 32;
	bl = b & 0xffffffff;
	bh = b >> 32;
	*lo = al*bl;
	m1 = ah*bl;
	m2 = al*bh;
	*hi = ah*bh + (m1>>32) + (m2>>32);
	t = *lo;
	*lo += m1<<32;
	*hi += *lo < t;
	t = *lo;
	*lo += m2<<32;
	*hi += *lo < t;
}

vvlong
__multi3(vvlong a, vvlong b)
{
	uvlong al, ah, bl, bh, hi, lo;
	unsigned __int128 r;

	al = (uvlong)a;
	ah = (uvlong)((unsigned __int128)a >> 64);
	bl = (uvlong)b;
	bh = (uvlong)((unsigned __int128)b >> 64);
	mul64(al, bl, &hi, &lo);
	hi += al*bh + ah*bl;
	r = (unsigned __int128)hi << 64 | lo;
	return (vvlong)r;
}
