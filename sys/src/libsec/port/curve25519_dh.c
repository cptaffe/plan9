#include "os.h"
#include <libsec.h>

static uchar nine[32] = {9};
static uchar zero[32];

/* RFC 7748's X25519: s clamped, u's top bit cleared, both as they were after; 0 if out is zero */
int
x25519(uchar out[32], uchar s[32], uchar u[32])
{
	uchar sf, sl, ul;

	sf = s[0];
	sl = s[31];
	ul = u[31];
	s[0] &= ~7;
	s[31] = 0x40 | (s[31] & 0x7f);
	u[31] &= 0x7f;
	curve25519(out, s, u);
	s[0] = sf;
	s[31] = sl;
	u[31] = ul;
	return tsmemcmp(out, zero, 32) != 0;
}

/* x a new secret, y its public value, whose unused top bit is random */
void
curve25519_dh_new(uchar x[32], uchar y[32])
{
	uchar b;

	genrandom(x, 32);
	b = x[31];
	x25519(y, x, nine);
	y[31] |= b & 0x80;
}

/* z the secret shared with y's owner; x and y are wiped; 0 if z is zero */
int
curve25519_dh_finish(uchar x[32], uchar y[32], uchar z[32])
{
	int r;

	y[31] &= 0x7f;
	r = x25519(z, x, y);
	memset(x, 0, 32);
	memset(y, 0, 32);
	return r;
}
