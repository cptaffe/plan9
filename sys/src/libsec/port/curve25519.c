#include "os.h"
#include <libsec.h>
#include "fe25519.h"

/*
 * mypublic = secret·basepoint on curve25519, by the Montgomery ladder, as donna's: the secret's
 * 256 bits as they are (x25519 clamps it), the base point's top bit not read
 */
void
curve25519(uchar mypublic[32], uchar secret[32], uchar basepoint[32])
{
	Fe x1, x2, z2, x3, z3, a, aa, b, bb, e, c, d, da, cb;
	uchar u[32];
	u32int bit, swap;
	int pos;

	memmove(u, basepoint, 32);
	u[31] &= 127;
	fefrombytes(x1, u);
	feset(x2, 1);
	feset(z2, 0);
	memmove(x3, x1, sizeof(Fe));
	feset(z3, 1);
	swap = 0;
	for(pos = 255; pos >= 0; pos--){
		bit = (secret[pos/8] >> (pos&7)) & 1;
		swap ^= bit;
		fecswap(x2, x3, swap);
		fecswap(z2, z3, swap);
		swap = bit;
		feadd(a, x2, z2);
		fesq(aa, a);
		fesub(b, x2, z2);
		fesq(bb, b);
		fesub(e, aa, bb);
		feadd(c, x3, z3);
		fesub(d, x3, z3);
		femul(da, d, a);
		femul(cb, c, b);
		feadd(x3, da, cb);
		fesq(x3, x3);
		fesub(z3, da, cb);
		fesq(z3, z3);
		femul(z3, z3, x1);
		femul(x2, aa, bb);
		femul121665(z2, e);
		feadd(z2, z2, aa);
		femul(z2, z2, e);
	}
	fecswap(x2, x3, swap);
	fecswap(z2, z3, swap);
	feinv(z2, z2);
	femul(x2, x2, z2);
	fetobytes(mypublic, x2);
	memset(x2, 0, sizeof x2);
	memset(z2, 0, sizeof z2);
	memset(x3, 0, sizeof x3);
	memset(z3, 0, sizeof z3);
}
