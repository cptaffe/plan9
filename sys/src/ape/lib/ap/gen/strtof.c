#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <errno.h>

extern char *_dtoa(double, int, int, int*, int*, char **);
extern int _strtonx(const char *, char **, int, double *, int, int);

/*
 * The decimal in s (strtod's syntax, as far as e), as 0.dig * 10^*dexp:
 * dig its significant digits, with no zeros at either end, as many as
 * ndig; *more says if there are others, not zero, after them.
 */
static int
digits(const char *s, const char *e, char *dig, int ndig, int *dexp, int *more)
{
	int n, x, xs, point, any;

	while(s < e && (isspace((unsigned char)*s) || *s == '+' || *s == '-'))
		s++;
	n = 0;
	point = 0;
	*dexp = 0;
	*more = 0;
	any = 0;
	for(; s < e && (isdigit((unsigned char)*s) || (*s == '.' && !point)); s++){
		if(*s == '.'){
			point = 1;
			continue;
		}
		if(*s == '0' && !any){	/* a leading zero */
			if(point)
				(*dexp)--;
			continue;
		}
		any = 1;
		if(!point)
			(*dexp)++;
		if(n < ndig)
			dig[n++] = *s;
		else if(*s != '0')
			*more = 1;
	}
	if(s < e && (*s == 'e' || *s == 'E')){
		s++;
		xs = 1;
		if(*s == '+' || *s == '-')
			xs = *s++ == '-'? -1: 1;
		for(x = 0; s < e && isdigit((unsigned char)*s); s++)
			if(x < 100000)
				x = x*10 + *s - '0';
		*dexp += xs*x;
	}
	while(n > 0 && dig[n-1] == '0')
		n--;
	dig[n] = 0;
	return n;
}

/*
 * The sign of |s| - |d|, d a double strtod made of s: its exact digits
 * (_dtoa's, to more than it has) against s's.
 */
static int
cmp(const char *s, const char *e, double d)
{
	static char dig[1100];
	char *dd, *end;
	int n, nd, dexp, decpt, sign, i, c, more;

	n = digits(s, e, dig, sizeof dig - 1, &dexp, &more);
	dd = _dtoa(fabs(d), 2, 1000, &decpt, &sign, &end);
	nd = end - dd;	/* not past it: the buffer has others' digits */
	if(n == 0)
		return -1;
	if(dexp != decpt)
		return dexp > decpt? 1: -1;
	for(i = 0; i < n || i < nd; i++){
		c = (i < n? dig[i]: '0') - (i < nd? dd[i]: '0');
		if(c != 0)
			return c;
	}
	return more;
}

/*
 * The float nearest the number: strtod's double, rounded.  That rounds
 * twice, which is wrong only if the double is halfway between floats:
 * then which side the number is of it decides.  Hex is rounded to a
 * float's bits at once.
 */
float
strtof(const char *s, char **ep)
{
	const char *p;
	char *e;
	double d, a, b;
	float f, fa, fb;
	int c;

	for(p = s; isspace((unsigned char)*p); p++)
		;
	c = *p == '-';
	if(*p == '+' || *p == '-')
		p++;
	if(_strtonx(p, &e, c, &d, 24, -126)){
		if(ep)
			*ep = e;
		f = d;
	}else{
		d = strtod(s, &e);
		if(ep)
			*ep = e;
		f = d;
		if(isfinite(d) && (double)f != d){
			/*
			 * the floats either side of d, fa and fb, and as doubles,
			 * a and b: past FLT_MAX is 2^128, which as a float is inf
			 */
			fa = f;
			a = isinf(fa)? copysign(0x1p128, d): fa;
			fb = isinf(fa)? copysign(FLT_MAX, d): nextafterf(fa, a < d? INFINITY: -INFINITY);
			b = isinf(fb)? copysign(0x1p128, d): fb;
			if((a + b)/2 == d && (c = cmp(s, e, d)) != 0){
				/* halfway, and the number is not: the float on its side */
				if((c > 0) == (fabs(b) > fabs(a)))
					f = fb;
				else
					f = fa;
			}
		}
	}
	if((isinf(f) && !isinf(d)) || (f == 0 && d != 0))
		errno = ERANGE;
	return f;
}

long double
strtold(const char *s, char **ep)
{
	return strtod(s, ep);
}
