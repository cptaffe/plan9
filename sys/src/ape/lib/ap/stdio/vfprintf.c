/*
 * pANS stdio -- vfprintf
 */
#include "iolib.h"
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <limits.h>
/*
 * Leading flags
 */
#define	SPACE	1		/* ' ' prepend space if no sign printed */
#define	ALT	2		/* '#' use alternate conversion */
#define	SIGN	4		/* '+' prepend sign, even if positive */
#define	LEFT	8		/* '-' left-justify */
#define	ZPAD	16		/* '0' zero-pad */
/*
 * Trailing flags
 */
#define	SHORT	32		/* 'h' convert a short integer */
#define	LONG	64		/* 'l' convert a long integer */
#define	LDBL	128		/* 'L' convert a long double */
#define	PTR	256		/*     convert a void * (%p) */
#define	VLONG	512		/* 'll' convert a long long integer; 'j' an intmax_t */
#define	CHAR	1024		/* 'hh' convert a char */
/* 'z' (size_t) and 't' (ptrdiff_t) are LONG */

static int lflag[] = {	/* leading flags */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^@ ^A ^B ^C ^D ^E ^F ^G */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^H ^I ^J ^K ^L ^M ^N ^O */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^P ^Q ^R ^S ^T ^U ^V ^W */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
SPACE,	0,	0,	ALT,	0,	0,	0,	0,	/* sp  !  "  #  $  %  &  ' */
0,	0,	0,	SIGN,	0,	LEFT,	0,	0,	/*  (  )  *  +  ,  -  .  / */
ZPAD,	0,	0,	0,	0,	0,	0,	0,	/*  0  1  2  3  4  5  6  7 */
0,	0,	0,	0,	0,	0,	0,	0,	/*  8  9  :  ;  <  =  >  ? */
0,	0,	0,	0,	0,	0,	0,	0,	/*  @  A  B  C  D  E  F  G */
0,	0,	0,	0,	0,	0,	0,	0,	/*  H  I  J  K  L  M  N  O */
0,	0,	0,	0,	0,	0,	0,	0,	/*  P  Q  R  S  T  U  V  W */
0,	0,	0,	0,	0,	0,	0,	0,	/*  X  Y  Z  [  \  ]  ^  _ */
0,	0,	0,	0,	0,	0,	0,	0,	/*  `  a  b  c  d  e  f  g */
0,	0,	0,	0,	0,	0,	0,	0,	/*  h  i  j  k  l  m  n  o */
0,	0,	0,	0,	0,	0,	0,	0,	/*  p  q  r  s  t  u  v  w */
0,	0,	0,	0,	0,	0,	0,	0,	/*  x  y  z  {  |  }  ~ ^? */

0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
};

static int tflag[] = {	/* trailing flags */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^@ ^A ^B ^C ^D ^E ^F ^G */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^H ^I ^J ^K ^L ^M ^N ^O */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^P ^Q ^R ^S ^T ^U ^V ^W */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
0,	0,	0,	0,	0,	0,	0,	0,	/* sp  !  "  #  $  %  &  ' */
0,	0,	0,	0,	0,	0,	0,	0,	/*  (  )  *  +  ,  -  .  / */
0,	0,	0,	0,	0,	0,	0,	0,	/*  0  1  2  3  4  5  6  7 */
0,	0,	0,	0,	0,	0,	0,	0,	/*  8  9  :  ;  <  =  >  ? */
0,	0,	0,	0,	0,	0,	0,	0,	/*  @  A  B  C  D  E  F  G */
0,	0,	0,	0,	LDBL,	0,	0,	0,	/*  H  I  J  K  L  M  N  O */
0,	0,	0,	0,	0,	0,	0,	0,	/*  P  Q  R  S  T  U  V  W */
0,	0,	0,	0,	0,	0,	0,	0,	/*  X  Y  Z  [  \  ]  ^  _ */
0,	0,	0,	0,	0,	0,	0,	0,	/*  `  a  b  c  d  e  f  g */
SHORT,	0,	VLONG,	0,	LONG,	0,	0,	0,	/*  h  i  j  k  l  m  n  o */
0,	0,	0,	0,	LONG,	0,	0,	0,	/*  p  q  r  s  t  u  v  w */
0,	0,	LONG,	0,	0,	0,	0,	0,	/*  x  y  z  {  |  }  ~ ^? */

0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
};

static int ocvt_A(FILE *, va_list *, int, int, int);
static int ocvt_F(FILE *, va_list *, int, int, int);
static int ocvt_a(FILE *, va_list *, int, int, int);
static int ocvt_E(FILE *, va_list *, int, int, int);
static int ocvt_G(FILE *, va_list *, int, int, int);
static int ocvt_X(FILE *, va_list *, int, int, int);
static int ocvt_c(FILE *, va_list *, int, int, int);
static int ocvt_d(FILE *, va_list *, int, int, int);
static int ocvt_e(FILE *, va_list *, int, int, int);
static int ocvt_f(FILE *, va_list *, int, int, int);
static int ocvt_g(FILE *, va_list *, int, int, int);
static int ocvt_n(FILE *, va_list *, int, int, int);
static int ocvt_o(FILE *, va_list *, int, int, int);
static int ocvt_p(FILE *, va_list *, int, int, int);
static int ocvt_s(FILE *, va_list *, int, int, int);
static int ocvt_u(FILE *, va_list *, int, int, int);
static int ocvt_x(FILE *, va_list *, int, int, int);

static int(*ocvt[])(FILE *, va_list *, int, int, int) = {
0,	0,	0,	0,	0,	0,	0,	0,	/* ^@ ^A ^B ^C ^D ^E ^F ^G */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^H ^I ^J ^K ^L ^M ^N ^O */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^P ^Q ^R ^S ^T ^U ^V ^W */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
0,	0,	0,	0,	0,	0,	0,	0,	/* sp  !  "  #  $  %  &  ' */
0,	0,	0,	0,	0,	0,	0,	0,	/*  (  )  *  +  ,  -  .  / */
0,	0,	0,	0,	0,	0,	0,	0,	/*  0  1  2  3  4  5  6  7 */
0,	0,	0,	0,	0,	0,	0,	0,	/*  8  9  :  ;  <  =  >  ? */
0,	ocvt_A,	0,	0,	0,	ocvt_E,	ocvt_F,	ocvt_G,	/*  @  A  B  C  D  E  F  G */
0,	0,	0,	0,	0,	0,	0,	0,	/*  H  I  J  K  L  M  N  O */
0,	0,	0,	0,	0,	0,	0,	0,	/*  P  Q  R  S  T  U  V  W */
ocvt_X,	0,	0,	0,	0,	0,	0,	0,	/*  X  Y  Z  [  \  ]  ^  _ */
0,	ocvt_a,	0,	ocvt_c,	ocvt_d,	ocvt_e,	ocvt_f,	ocvt_g,	/*  `  a  b  c  d  e  f  g */
0,	ocvt_d,	0,	0,	0,	0,	ocvt_n,	ocvt_o,	/*  h  i  j  k  l  m  n  o */
ocvt_p,	0,	0,	ocvt_s,	0,	ocvt_u,	0,	0,	/*  p  q  r  s  t  u  v  w */
ocvt_x,	0,	0,	0,	0,	0,	0,	0,	/*  x  y  z  {  |  }  ~ ^? */

0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
0,	0,	0,	0,	0,	0,	0,	0,
};

static _Thread_local int nprint;
/*
 * In wprintf's conversions (_vfprintfw), widths and precisions
 * count characters, not bytes: the bytes are UTF-8.
 */
static _Thread_local int widemode;

int
_vfprintfw(FILE *f, const char *as, va_list args)
{
	int n;

	widemode = 1;
	n = vfprintf(f, as, args);
	widemode = 0;
	return n;
}

static int
uvfprintf(FILE *f, const char *as, va_list args)
{
	int tfl, flags, width, precision;
	unsigned char *s;

	nprint = 0;
	s = (unsigned char *)as;
	while(*s){
		if(*s != '%'){
			putc(*s++, f);
			nprint++;
			continue;
		}
		s++;
		flags = 0;
		while(lflag[*s]) flags |= lflag[*s++];
		if(*s == '*'){
			width = va_arg(args, int);
			s++;
			if(width<0){
				flags |= LEFT;
				width = -width;
			}
		}
		else{
			width = 0;
			while('0'<=*s && *s<='9') width = width*10 + *s++ - '0';
		}
		if(*s == '.'){
			s++;
			if(*s == '*'){
				precision = va_arg(args, int);
				s++;
			}
			else{
				precision = 0;
				while('0'<=*s && *s<='9') precision = precision*10 + *s++ - '0';
			}
		}
		else
			precision = -1;
		while(tfl = tflag[*s]){
			if(tfl == LONG && (flags & LONG)){
				flags &= ~LONG;
				tfl = VLONG;
			}
			if(tfl == SHORT && (flags & SHORT)){
				flags &= ~SHORT;
				tfl = CHAR;
			}
			flags |= tfl;
			s++;
		}
		if(ocvt[*s]) nprint += (*ocvt[*s++])(f, &args, flags, width, precision);
		else if(*s){
			putc(*s++, f);
			nprint++;
		}
	}
	return ferror(f)? -1: nprint;;
}

static int
ocvt_c(FILE *f, va_list *args, int flags, int width, int precision)
{
	int i;

	char b[MB_LEN_MAX];
	int n, m;
	mbstate_t st;

	USED(precision);
	if(flags&LONG){	/* a wint_t, in UTF-8 */
		memset(&st, 0, sizeof st);
		n = wcrtomb(b, va_arg(*args, wint_t), &st);
		if(n < 0)
			return -1;
	}else{
		b[0] = va_arg(*args, int);
		n = 1;
	}
	m = widemode? 1: n;	/* the width it fills */
	if(!(flags&LEFT)) for(i=m; i<width; i++) putc(' ', f);
	fwrite(b, 1, n, f);
	if(flags&LEFT) for(i=m; i<width; i++) putc(' ', f);
	return width<m ? n : n + width - m;
}

static int
ocvt_s(FILE *f, va_list *args, int flags, int width, int precision)
{
	char *s, *ms, b[MB_LEN_MAX];
	wchar_t *w;
	const wchar_t *wp;
	size_t k;
	int i, n, len, nc, m;

	ms = 0;
	if(flags&LONG){	/* wide characters, in UTF-8 */
		w = va_arg(*args, wchar_t *);
		if(!w)
			w = L"";
		k = 0;
		for(i = 0; w[i] && (precision < 0 || (widemode? i < precision: 1)); i++){
			m = wctomb(b, w[i]);
			if(m < 0)
				return -1;
			if(!widemode && precision >= 0 && k + m > precision)
				break;	/* a precision of bytes, of whole characters */
			k += m;
		}
		ms = malloc(k+1);
		if(ms == 0)
			return -1;
		wp = w;
		wcsrtombs(ms, &wp, k, 0);
		ms[k] = 0;
		s = ms;
		precision = -1;
	}else
		s = va_arg(*args, char *);
	if(!s)
		s = "";
	/* len bytes of s, which are nc characters */
	len = nc = 0;
	while(s[len] && (precision < 0 || nc < precision)){
		m = 1;
		if(widemode && (m = mblen(s+len, MB_LEN_MAX)) <= 0)
			m = 1;
		len += m;
		nc++;
	}
	n = 0;
	if(!(flags&LEFT))
		for(; nc < width; nc++, n++)
			putc(' ', f);
	fwrite(s, 1, len, f);
	n += len;
	if(flags&LEFT)
		for(; nc < width; nc++, n++)
			putc(' ', f);
	free(ms);
	return n;
}

static int
ocvt_n(FILE *f, va_list *args, int flags, int width, int precision)
{
	USED(f, width, precision);
	if(flags&CHAR)
		*va_arg(*args, signed char *) = nprint;
	else if(flags&SHORT)
		*va_arg(*args, short *) = nprint;
	else if(flags&LONG)
		*va_arg(*args, long *) = nprint;
	else if(flags&VLONG)
		*va_arg(*args, long long*) = nprint;
	else
		*va_arg(*args, int *) = nprint;
	return 0;
}

/*
 * Generic fixed-point conversion
 *	f is the output FILE *;
 *	args is the va_list * from which to get the number;
 *	flags, width and precision are the results of printf-cracking;
 *	radix is the number base to print in;
 *	alphabet is the set of digits to use;
 *	prefix is the prefix to print before non-zero numbers when
 *	using ``alternate form.''
 */
static int
ocvt_fixed(FILE *f, va_list *args, int flags, int width, int precision,
	int radix, int sgned, char alphabet[], char *prefix)
{
	char digits[128];	/* no reasonable machine will ever overflow this */
	char *sign;
	char *dp;
	long long snum;
	unsigned long long num;
	int nout, npad, nlzero;

	if(sgned){
		if(flags&PTR) snum = /* (intptr_t) */ (long long)
			va_arg(*args, void *);
		else if(flags&CHAR) snum = (signed char)va_arg(*args, int);
		else if(flags&SHORT) snum = va_arg(*args, short);
		else if(flags&LONG) snum = va_arg(*args, long);
		else if(flags&VLONG) snum = va_arg(*args, long long);
		else snum = va_arg(*args, int);
		if(snum < 0){
			sign = "-";
			num = -snum;
		} else{
			if(flags&SIGN) sign = "+";
			else if(flags&SPACE) sign = " ";
			else sign = "";
			num = snum;
		}
	} else {
		sign = "";
		if(flags&PTR) num = /* (uintptr_t) */ (unsigned long long)
			va_arg(*args, void *);
		else if(flags&CHAR) num = (unsigned char)va_arg(*args, unsigned);
		else if(flags&SHORT) num = va_arg(*args, unsigned short);
		else if(flags&LONG) num = va_arg(*args, unsigned long);
		else if(flags&VLONG) num = va_arg(*args, unsigned long long);
		else num = va_arg(*args, unsigned int);
	}
	if(num == 0) prefix = "";
	dp = digits;
	do{
		*dp++ = alphabet[num%radix];
		num /= radix;
	}while(num);
	if(precision==0 && dp-digits==1 && dp[-1]=='0')
		dp--;
	if(flags&ALT && radix==8 && dp==digits)	/* ANSI: %#o begins with a 0 */
		*dp++ = '0';
	nlzero = precision-(dp-digits);
	if(nlzero < 0) nlzero = 0;
	if(flags&ALT){
		if(radix == 8) if(dp[-1]=='0' || nlzero) prefix = "";
	}
	else prefix = "";
	nout = dp-digits+nlzero+strlen(prefix)+strlen(sign);
	npad = width-nout;
	if(npad < 0) npad = 0;
	nout += npad;
	if(!(flags&LEFT)){
		if(flags&ZPAD && precision < 0){	/* ANSI: a precision overrides 0 */
			fputs(sign, f);
			fputs(prefix, f);
			while(npad){
				putc('0', f);
				--npad;
			}
		} else{
			while(npad){
				putc(' ', f);
				--npad;
			}
			fputs(sign, f);
			fputs(prefix, f);
		}
		while(nlzero){
			putc('0', f);
			--nlzero;
		}
		while(dp!=digits) putc(*--dp, f);
	}
	else{
		fputs(sign, f);
		fputs(prefix, f);
		while(nlzero){
			putc('0', f);
			--nlzero;
		}
		while(dp != digits) putc(*--dp, f);
		while(npad){
			putc(' ', f);
			--npad;
		}
	}
	return nout;
}

static int
ocvt_X(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags, width, precision, 16, 0, "0123456789ABCDEF", "0X");
}

static int
ocvt_d(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags, width, precision, 10, 1, "0123456789", "");
}

static int
ocvt_o(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags, width, precision, 8, 0, "01234567", "0");
}

static int
ocvt_p(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags|PTR|ALT, width, precision, 16, 0,
		"0123456789ABCDEF", "0x");
}

static int
ocvt_u(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags, width, precision, 10, 0, "0123456789", "");
}

static int
ocvt_x(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_fixed(f, args, flags, width, precision, 16, 0, "0123456789abcdef", "0x");
}

static int ocvt_flt(FILE *, va_list *, int, int, int, char);

static int
ocvt_E(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'E');
}

static int
ocvt_G(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'G');
}

static int
ocvt_e(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'e');
}

static int
ocvt_f(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'f');
}

static int
ocvt_g(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'g');
}

static int
ocvt_F(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_flt(f, args, flags, width, precision, 'F');
}

/*
 * C99: an infinity or NaN is inf or nan, INF or NAN for the
 * upper-case conversions, padded with spaces
 */
static int
ocvt_inf(FILE *f, double d, int flags, int width, int upper)
{
	char *s, *sign;
	int n;

	s = isnan(d)? (upper? "NAN": "nan"): (upper? "INF": "inf");
	sign = signbit(d)? "-": flags&SIGN? "+": flags&SPACE? " ": "";
	n = strlen(sign) + 3;
	if(!(flags&LEFT))
		for(; n < width; n++)
			putc(' ', f);
	fputs(sign, f);
	fputs(s, f);
	for(; n < width; n++)
		putc(' ', f);
	return n;
}

/*
 * C99's %a: 0x1.hhhp+d, the hex digits of the fraction (0x0.hhhp-1022
 * if subnormal), as many as precision, rounded to even, or else
 * as many as it takes
 */
static int
ocvt_hex(FILE *f, va_list *args, int flags, int width, int precision, int upper)
{
	char body[32], ebuf[16], *sign, *prefix, *xd, *ep;
	union { double d; unsigned long long u; } a;
	unsigned long long frac, lead, half, rest;
	int e, nd, n, i, k, z;
	double d;

	d = va_arg(*args, double);
	if(!isfinite(d))
		return ocvt_inf(f, d, flags, width, upper);
	xd = upper? "0123456789ABCDEF": "0123456789abcdef";
	a.d = d;
	sign = a.u>>63? "-": flags&SIGN? "+": flags&SPACE? " ": "";
	prefix = upper? "0X": "0x";
	frac = a.u & ((1ULL<<52)-1);
	e = (a.u>>52) & 0x7ff;
	lead = e != 0;
	if(e == 0)
		e = frac? -1022: 0;
	else
		e -= 1023;
	nd = 13;
	if(precision >= 0 && precision < 13){
		k = 4*(13-precision);	/* bits to lose */
		half = frac>>(k-1) & 1;
		rest = frac & ((1ULL<<(k-1))-1);
		frac >>= k;
		if(half && (rest || (precision? frac & 1: lead & 1)))	/* even, in the last digit kept */
			frac++;
		if(frac >> 4*precision){	/* carried into the lead digit */
			frac &= (1ULL << 4*precision) - 1;
			lead++;
		}
		frac <<= k;
		nd = precision;
	}else if(precision < 0)
		for(nd = 13; nd > 0 && (frac>>4*(13-nd) & 0xf) == 0; nd--)
			;
	n = 0;
	body[n++] = xd[lead];
	if(nd > 0 || flags&ALT)
		body[n++] = '.';
	for(i = 0; i < nd && i < 13; i++)
		body[n++] = xd[frac>>(48-4*i) & 0xf];
	body[n] = 0;
	z = precision > 13? precision - 13: 0;	/* zeros past the 13 digits there are */
	ep = ebuf + sizeof ebuf;
	*--ep = 0;
	k = e < 0? -e: e;
	do
		*--ep = '0' + k%10;
	while(k /= 10);
	*--ep = e < 0? '-': '+';
	*--ep = upper? 'P': 'p';
	n += z + strlen(ep) + strlen(sign) + 2;
	if(!(flags&LEFT) && !(flags&ZPAD))
		for(; n < width; n++)
			putc(' ', f);
	fputs(sign, f);
	fputs(prefix, f);
	if(!(flags&LEFT) && flags&ZPAD)
		for(; n < width; n++)
			putc('0', f);
	fputs(body, f);
	while(z-- > 0)
		putc('0', f);
	fputs(ep, f);
	for(; n < width; n++)
		putc(' ', f);
	return n;
}

static int
ocvt_a(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_hex(f, args, flags, width, precision, 0);
}

static int
ocvt_A(FILE *f, va_list *args, int flags, int width, int precision)
{
	return ocvt_hex(f, args, flags, width, precision, 1);
}

static int
ocvt_flt(FILE *f, va_list *args, int flags, int width, int precision, char afmt)
{
	extern char *_dtoa(double, int, int, int*, int*, char **);
	int echr;
	char *digits, *edigits;
	int exponent;
	char fmt;
	int sign;
	int ndig;
	int nout, i;
	char ebuf[20];	/* no sensible machine will overflow this */
	char *eptr;
	double d;

	digits = eptr = 0;
	echr = 'e';
	fmt = afmt;
	d = va_arg(*args, double);
	if(!isfinite(d))
		return ocvt_inf(f, d, flags, width, afmt=='E' || afmt=='G' || afmt=='F');
	if(fmt == 'F')
		fmt = 'f';
	if(precision < 0) precision = 6;
	switch(fmt){
	case 'f':
		digits = _dtoa(d, 3, precision, &exponent, &sign, &edigits);
		break;
	case 'E':
		echr = 'E';
		fmt = 'e';
		/* fall through */
	case 'e':
		digits = _dtoa(d, 2, 1+precision, &exponent, &sign, &edigits);
		break;
	case 'G':
		echr = 'E';
		/* fall through */
	case 'g':
		if (precision == 0)	/* ANSI: as 1 */
			precision = 1;
		if (precision > 0)
			digits = _dtoa(d, 2, precision, &exponent, &sign, &edigits);
		else {
			digits = _dtoa(d, 0, precision, &exponent, &sign, &edigits);
			precision = edigits - digits;
			if (exponent > precision && exponent <= precision + 4)
				precision = exponent;
			}
		if(exponent >= -3 && exponent <= precision){
			fmt = 'f';
			precision -= exponent;
		}else{
			fmt = 'e';
			--precision;
		}
		break;
	}
	if (exponent == 9999) {
		/* Infinity or Nan */
		precision = 0;
		exponent = edigits - digits;
		fmt = 'f';
	}
	ndig = edigits-digits;
	if(ndig == 0) {
		ndig = 1;
		digits = "0";
	}
	if((afmt=='g' || afmt=='G') && !(flags&ALT)){	/* knock off trailing zeros */
		if(fmt == 'f'){
			if(precision+exponent > ndig) {
				precision = ndig - exponent;
				if(precision < 0)
					precision = 0;
			}
		}
		else{
			if(precision > ndig-1) precision = ndig-1;
		}
	}
	nout = precision;				/* digits after decimal point */
	if(precision!=0 || flags&ALT) nout++;		/* decimal point */
	if(fmt=='f' && exponent>0) nout += exponent;	/* digits before decimal point */
	else nout++;					/* there's always at least one */
	if(sign || flags&(SPACE|SIGN)) nout++;		/* sign */
	if(fmt != 'f'){					/* exponent */
		eptr = ebuf;
		for(i=exponent<=0?1-exponent:exponent-1; i; i/=10)
			*eptr++ = '0' + i%10;
		while(eptr<ebuf+2) *eptr++ = '0';
		nout += eptr-ebuf+2;			/* e+99 */
	}
	if(!(flags&ZPAD) && !(flags&LEFT))
		while(nout < width){
			putc(' ', f);
			nout++;
		}
	if(sign) putc('-', f);
	else if(flags&SIGN) putc('+', f);
	else if(flags&SPACE) putc(' ', f);
	if((flags&ZPAD) && !(flags&LEFT))
		while(nout < width){
			putc('0', f);
			nout++;
		}
	if(fmt == 'f'){
		for(i=0; i<exponent; i++) putc(i<ndig?digits[i]:'0', f);
		if(i == 0) putc('0', f);
		if(precision>0 || flags&ALT) putc('.', f);
		for(i=0; i!=precision; i++)
			putc(0<=i+exponent && i+exponent<ndig?digits[i+exponent]:'0', f);
	}
	else{
		putc(digits[0], f);
		if(precision>0 || flags&ALT) putc('.', f);
		for(i=0; i!=precision; i++) putc(i<ndig-1?digits[i+1]:'0', f);
	}
	if(fmt != 'f'){
		putc(echr, f);
		putc(exponent<=0?'-':'+', f);
		while(eptr>ebuf) putc(*--eptr, f);
	}
	while(nout < width){
		putc(' ', f);
		nout++;
	}
	return nout;
}

int
vfprintf(FILE *f, const char *as, va_list args)
{
	int r;

	flockfile(f);
	r = uvfprintf(f, as, args);
	funlockfile(f);
	return r;
}
