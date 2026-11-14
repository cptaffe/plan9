/*
 * GF(2^255-19), for curve25519.c and ed25519.c: an element is 32 limbs of 8 bits, little-endian,
 * after Bernstein's and Dempsky's reference curve25519 (public domain). Every result is squeezed,
 * its limbs below 256 and the top one below 129, which fesub's 2p needs of its second operand; it
 * may be above p until fefreeze makes it the least one.
 */
typedef u32int Fe[32];

void	feadd(Fe, Fe, Fe);
void	fesub(Fe, Fe, Fe);
void	femul(Fe, Fe, Fe);
void	fesq(Fe, Fe);
void	femul121665(Fe, Fe);
void	fefreeze(Fe);
void	feinv(Fe, Fe);
void	fepow22523(Fe, Fe);
void	fecswap(Fe, Fe, u32int);
void	fefrombytes(Fe, uchar*);
void	fetobytes(uchar*, Fe);
void	feset(Fe, u32int);
