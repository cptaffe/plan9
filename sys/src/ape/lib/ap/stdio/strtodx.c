#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

/*
 * The forms of a number C99 adds to strtod's: infinity, inf, nan,
 * nan(chars), and 0xh.hhhp±d, in hex, rounded once, to nearest.
 * s is past the sign; *d is the number, and the result 1, if
 * it is one of them.
 */
typedef unsigned long long uvlong;

static int
prefix(const char *s, char *w)	/* the length of w, if s starts with it, any case */
{
	int i;

	for(i = 0; w[i]; i++)
		if(tolower((unsigned char)s[i]) != w[i])
			return 0;
	return i;
}

static int
hexdig(int c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* the number, rounded to mbits bits (fewer below 2^emin, as a subnormal is) */
static double
hex(const char *s, const char **ep, int mbits, int emin)
{
	uvlong m, half, rest;
	int d, e, x, xs, any, top, sh;
	double r;

	m = 0;
	e = 0;	/* the number is m * 2^e, the dropped digits sticky in m's bit 0 */
	any = 0;
	for(; (d = hexdig(*s)) >= 0; s++, any = 1)
		if(m >> 60)
			m |= d != 0, e += 4;
		else
			m = m<<4 | d;
	if(*s == '.'){
		for(s++; (d = hexdig(*s)) >= 0; s++, any = 1)
			if(m >> 60)
				m |= d != 0;
			else{
				m = m<<4 | d;
				e -= 4;
			}
	}
	if(!any)
		return -1;	/* not hex: only the 0 is a number */
	*ep = s;
	if(*s == 'p' || *s == 'P'){
		s++;
		xs = 1;
		if(*s == '+' || *s == '-')
			xs = *s++ == '-'? -1: 1;
		if(isdigit((unsigned char)*s)){
			for(x = 0; isdigit((unsigned char)*s); s++)
				if(x < 100000)
					x = x*10 + *s - '0';
			e += xs*x;
			*ep = s;
		}
	}
	if(m == 0)
		return 0;
	for(top = 63; (m >> top) == 0; top--)
		;
	/* bits kept: mbits, or fewer for a subnormal (its lead bit below 2^emin) */
	sh = top - (mbits-1);
	if(top + e < emin)
		sh += emin - (top + e);
	if(sh > 0){
		if(sh > 64){
			m = 0;
			e += sh;
		}else if(sh == 64){	/* all go: m rounds to 0 or 1 */
			half = m >> 63;
			rest = m & ((1ULL<<63)-1);
			m = half && rest;
			e += sh;
		}else{
			half = m>>(sh-1) & 1;
			rest = m & ((1ULL<<(sh-1))-1);
			m >>= sh;
			e += sh;
			if(half && (rest || (m & 1)))
				m++;
		}
	}
	r = ldexp((double)m, e);
	if(r == 0 || isinf(r))
		errno = ERANGE;
	return r;
}

/*
 * mbits and emin are the precision and least normal exponent of the
 * type: a double's, 53 and -1022; a float's, 24 and -126.
 */
int
_strtonx(const char *s, char **se, int neg, double *d, int mbits, int emin)
{
	const char *e;
	double r;
	int n;

	if((n = prefix(s, "infinity")) || (n = prefix(s, "inf"))){
		r = HUGE_VAL;
		e = s + n;
	}else if((n = prefix(s, "nan")) != 0){
		r = nan("");	/* quiet */
		e = s + n;
		if(*e == '('){
			for(n = 1; isalnum((unsigned char)e[n]) || e[n] == '_'; n++)
				;
			if(e[n] == ')')
				e += n+1;
		}
	}else if(s[0] == '0' && (s[1] == 'x' || s[1] == 'X')){
		r = hex(s+2, &e, mbits, emin);
		if(r < 0){	/* "0x" not followed by hex: the number is 0 */
			r = 0;
			e = s+1;
		}
	}else
		return 0;
	if(se)
		*se = (char*)e;
	*d = neg? -r: r;
	return 1;
}

int
_strtodx(const char *s, char **se, int neg, double *d)
{
	return _strtonx(s, se, neg, d, 53, -1022);
}
