/*
 * pANS stdio -- vfscanf
 */
#include "iolib.h"
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>
static int icvt_f(FILE *f, va_list *args, int store, int width, int type);
static int icvt_x(FILE *f, va_list *args, int store, int width, int type);
static int icvt_sq(FILE *f, va_list *args, int store, int width, int type);
static int icvt_c(FILE *f, va_list *args, int store, int width, int type);
static int icvt_d(FILE *f, va_list *args, int store, int width, int type);
static int icvt_i(FILE *f, va_list *args, int store, int width, int type);
static int icvt_n(FILE *f, va_list *args, int store, int width, int type);
static int icvt_o(FILE *f, va_list *args, int store, int width, int type);
static int icvt_p(FILE *f, va_list *args, int store, int width, int type);
static int icvt_s(FILE *f, va_list *args, int store, int width, int type);
static int icvt_u(FILE *f, va_list *args, int store, int width, int type);
static int (*icvt[])(FILE *, va_list *, int, int, int)={
0,	0,	0,	0,	0,	0,	0,	0,	/* ^@ ^A ^B ^C ^D ^E ^F ^G */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^H ^I ^J ^K ^L ^M ^N ^O */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^P ^Q ^R ^S ^T ^U ^V ^W */
0,	0,	0,	0,	0,	0,	0,	0,	/* ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
0,	0,	0,	0,	0,	0,	0,	0,	/* sp  !  "  #  $  %  &  ' */
0,	0,	0,	0,	0,	0,	0,	0,	/*  (  )  *  +  ,  -  .  / */
0,	0,	0,	0,	0,	0,	0,	0,	/*  0  1  2  3  4  5  6  7 */
0,	0,	0,	0,	0,	0,	0,	0,	/*  8  9  :  ;  <  =  >  ? */
0,	icvt_f,	0,	0,	0,	icvt_f,	icvt_f,	icvt_f,	/*  @  A  B  C  D  E  F  G */
0,	0,	0,	0,	0,	0,	0,	0,	/*  H  I  J  K  L  M  N  O */
0,	0,	0,	0,	0,	0,	0,	0,	/*  P  Q  R  S  T  U  V  W */
icvt_x,	0,	0,	icvt_sq,0,	0,	0,	0,	/*  X  Y  Z  [  \  ]  ^  _ */
0,	icvt_f,	0,	icvt_c,	icvt_d,	icvt_f,	icvt_f,	icvt_f,	/*  `  a  b  c  d  e  f  g */
0,	icvt_i,	0,	0,	0,	0,	icvt_n,	icvt_o,	/*  h  i  j  k  l  m  n  o */
icvt_p,	0,	0,	icvt_s,	0,	icvt_u,	0,	0,	/*  p  q  r  s  t  u  v  w */
icvt_x,	0,	0,	0,	0,	0,	0,	0,	/*  x  y  z  {  |  }  ~ ^? */

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
#define	ngetc(f)		(nread++, getc(f))
#define	nungetc(c, f)		(--nread, ungetc((c), f))
#define	wgetc(c, f, out)	if(width--==0) goto out; (c)=ngetc(f)
#define	wungetc(c, f)		(++width, nungetc(c, f))
static int nread, ncvt;
static const char *fmtp;
/*
 * In wscanf's conversions (_vfscanfw), and for those of wide
 * characters (%lc, %ls, %l[), a character is read whole, its UTF-8
 * bytes at once, and a width counts characters.
 */
static int widemode;

int
_vfscanfw(FILE *f, const char *s, va_list args)
{
	int n;

	widemode = 1;
	n = vfscanf(f, s, args);
	widemode = 0;
	return n;
}

/* the bytes of the character c starts, into b; their number */
static int
getmb(FILE *f, int c, char *b)
{
	int n, k;

	b[0] = c;
	n = 1;
	k = c >= 0xF0? 3: c >= 0xE0? 2: c >= 0xC0? 1: 0;
	while(k-- > 0){
		c = ngetc(f);
		if(c == EOF || (c & 0xC0) != 0x80){
			if(c != EOF)
				nungetc(c, f);
			break;
		}
		b[n++] = c;
	}
	b[n] = 0;
	return n;
}

/* store the character in b: its bytes, or as a wchar_t; 0 if it is not one */
static int
putmb(char **s, wchar_t **ws, char *b, int n)
{
	wchar_t w;

	if(ws && *ws){
		if(mbtowc(&w, b, n) < 0)
			return 0;
		*(*ws)++ = w;
	}else if(s && *s){
		memmove(*s, b, n);
		*s += n;
	}
	return 1;
}

int vfscanf(FILE *f, const char *s, va_list args){
	int c, width, type, store;
	nread=0;
	ncvt=0;
	fmtp=s;
	for(;*fmtp;fmtp++) switch(*fmtp){
	default:
		if(isspace((unsigned char)*fmtp)){
			do
				c=ngetc(f);
			while(isspace(c));
			if(c==EOF) return ncvt?ncvt:EOF;
			nungetc(c, f);
			break;
		}
	NonSpecial:
		c=ngetc(f);
		if(c==EOF) return ncvt?ncvt:EOF;
		if(c!=*fmtp){
			nungetc(c, f);
			return ncvt;
		}
		break;
	case '%':
		fmtp++;
		if(*fmtp!='*') store=1;
		else{
			store=0;
			fmtp++;
		}
		if('0'<=*fmtp && *fmtp<='9'){
			width=0;
			while('0'<=*fmtp && *fmtp<='9') width=width*10 + *fmtp++ - '0';
		}
		else
			width=-1;
		/* C99: hh is H, ll and j are q, z and t are l */
		type='n';
		if(*fmtp=='h'){
			type=*++fmtp=='h'? (fmtp++, 'H'): 'h';
		}else if(*fmtp=='l'){
			type=*++fmtp=='l'? (fmtp++, 'q'): 'l';
		}else if(*fmtp=='j'){
			type='q';
			fmtp++;
		}else if(*fmtp=='z' || *fmtp=='t'){
			type='l';
			fmtp++;
		}else if(*fmtp=='L')
			type=*fmtp++;
		if(!icvt[*fmtp]) goto NonSpecial;
		/* the end of the input before the conversion begins is an input failure, EOF */
		c=ngetc(f);
		if(*fmtp!='c' && *fmtp!='[' && *fmtp!='n')
			while(isspace(c)) c=ngetc(f);
		if(c==EOF && *fmtp!='n') return ncvt?ncvt:EOF;
		nungetc(c, f);
		/* and a conversion that fails after that is a matching failure: the count */
		if(!(*icvt[*fmtp])(f, &args, store, width, type))
			return ncvt;
		if(*fmtp=='\0') break;
		if(store) ncvt++;
	}
	return ncvt;
}
static int icvt_n(FILE *f, va_list *args, int store, int width, int type){
	USED(f, width);
	if(store){
		--ncvt;	/* this assignment doesn't count! */
		switch(type){
		case 'H': *va_arg(*args, signed char *)=nread; break;
		case 'h': *va_arg(*args, short *)=nread; break;
		case 'q': *va_arg(*args, long long *)=nread; break;
		case 'n': *va_arg(*args, int *)=nread; break;
		case 'l':
		case 'L': *va_arg(*args, long *)=nread; break;
		}
	}
	return 1;
}
#define	SIGNED		1
#define	UNSIGNED	2
#define	POINTER		3
/*
 * Generic fixed-point conversion
 *	f is the input FILE *;
 *	args is the va_list * into which to store the number;
 *	store is a flag to enable storing;
 *	width is the maximum field width;
 *	type is 'h' 'l' or 'L', the scanf type modifier;
 *	unsgned is SIGNED, UNSIGNED or POINTER, giving part of the type to store in;
 *	base is the number base -- if 0, C number syntax is used.
 */
static int icvt_fixed(FILE *f, va_list *args,
				int store, int width, int type, int unsgned, int base){
	unsigned long long num=0;
	int sign=1, ndig=0, dig;
	int c;
	do
		c=ngetc(f);
	while(isspace(c));
	if(width--==0){
		nungetc(c, f);
		goto Done;
	}
	if(c=='+'){
		wgetc(c, f, Done);
	}
	else if(c=='-'){
		sign=-1;
		wgetc(c, f, Done);
	}
	switch(base){
	case 0:
		if(c=='0'){
			wgetc(c, f, Done);
			if(c=='x' || c=='X'){
				wgetc(c, f, Done);
				base=16;
			}
			else{
				ndig=1;
				base=8;
			}
		}
		else
			base=10;
		break;
	case 16:
		if(c=='0'){
			wgetc(c, f, Done);
			if(c=='x' || c=='X'){
				wgetc(c, f, Done);
			}
			else ndig=1;
		}
		break;
	}
	while('0'<=c && c<='9' || 'a'<=c && c<='f' || 'A'<=c && c<='F'){
		dig='0'<=c && c<='9'?c-'0':'a'<=c && c<='f'?c-'a'+10:c-'A'+10;
		if(dig>=base) break;
		ndig++;
		num=num*base+dig;
		wgetc(c, f, Done);
	}
	nungetc(c, f);
Done:
	if(ndig==0) return 0;
	if(store){
		switch(unsgned){
		case SIGNED:
			switch(type){
			case 'H': *va_arg(*args,  signed char *)=num*sign; break;
			case 'h': *va_arg(*args,  short *)=num*sign; break;
			case 'q': *va_arg(*args,  long long *)=num*sign; break;
			case 'n': *va_arg(*args,  int *)=num*sign; break;
			case 'l':
			case 'L': *va_arg(*args,  long *)=num*sign; break;
			}
			break;
		case UNSIGNED:
			switch(type){
			case 'H': *va_arg(*args, unsigned char *)=num*sign; break;
			case 'h': *va_arg(*args, unsigned short *)=num*sign; break;
			case 'q': *va_arg(*args, unsigned long long *)=num*sign; break;
			case 'n': *va_arg(*args, unsigned int *)=num*sign; break;
			case 'l':
			case 'L': *va_arg(*args, unsigned long *)=num*sign; break;
			}
			break;
		case POINTER:
			*va_arg(*args, void **)=(void *)(num*sign); break;
		}
	}
	return 1;
}
static int icvt_d(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, SIGNED, 10);
}
static int icvt_x(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, UNSIGNED, 16);
}
static int icvt_o(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, UNSIGNED, 8);
}
static int icvt_i(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, SIGNED, 0);
}
static int icvt_u(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, UNSIGNED, 10);
}
static int icvt_p(FILE *f, va_list *args, int store, int width, int type){
	return icvt_fixed(f, args, store, width, type, POINTER, 16);
}
#define	NBUF	509
/* whether b, so far, begins w, in any case */
static int prefix(const char *b, const char *w){
	for(; *b; b++, w++)
		if(tolower((unsigned char)*b)!=*w) return 0;
	return 1;
}
/*
 * C99: a number is strtod's: decimal, hex (0x...p...), inf, infinity, nan or nan(...)
 */
static int icvt_f(FILE *f, va_list *args, int store, int width, int type){
	char buf[NBUF+1];
	char *s=buf, *b, *e;
	int c, ndig=0, ndpt=0, nexp=1, hex=0, n;
	double d;
	if(width<0 || NBUF<width) width=NBUF;	/* bug -- no limit specified in ansi */
	do
		c=ngetc(f);
	while(isspace(c));
	if(width--==0){
		nungetc(c, f);
		goto Done;
	}
	if(c=='+' || c=='-'){
		*s++=c;
		wgetc(c, f, Done);
	}
	b=s;
	if(c=='i' || c=='I' || c=='n' || c=='N'){
		/* the longest prefix of infinity, or of nan */
		for(;;){
			s[0]=c;
			s[1]='\0';
			if(!prefix(b, "infinity") && !prefix(b, "nan")){
				nungetc(c, f);
				break;
			}
			s++;
			if(width--==0) break;
			c=ngetc(f);
		}
		*s='\0';
		n=strlen(b);
		if(n==3 && prefix(b, "nan") && width!=0){
			c=ngetc(f);
			if(c!='('){
				nungetc(c, f);
				goto Num;
			}
			*s++=c;
			for(;;){
				c=ngetc(f);
				if(!isalnum(c) && c!='_') break;
				if(s<buf+NBUF-1) *s++=c;
			}
			if(c!=')') return 0;
			*s++=c;
			*s='\0';
			goto Num;
		}
		if(n!=3 && n!=8) return 0;
		goto Num;
	}
	if(c=='0'){
		*s++=c;
		ndig++;
		wgetc(c, f, Done);
		if(c=='x' || c=='X'){
			*s++=c;
			hex=1;
			wgetc(c, f, Done);
		}
	}
	while(hex? isxdigit(c) || ndpt==0 && c=='.': '0'<=c && c<='9' || ndpt==0 && c=='.'){
		if(c=='.') ndpt++;
		else ndig++;
		*s++=c;
		wgetc(c, f, Done);
	}
	if(hex? c=='p' || c=='P': c=='e' || c=='E'){
		*s++=c;
		nexp=0;
		wgetc(c, f, Done);
		if(c=='+' || c=='-'){
			*s++=c;
			wgetc(c, f, Done);
		}
		while('0'<=c && c<='9'){
			*s++=c;
			nexp++;
			wgetc(c, f, Done);
		}
	}
	nungetc(c, f);
Done:
	if(ndig==0 || nexp==0) return 0;
	*s='\0';
Num:
	d=strtod(buf, &e);
	if(e==buf) return 0;
	if(store) switch(type){
	case 'h':
	case 'n': *va_arg(*args, float *)=d; break;
	case 'L': /* long double is double */
	case 'l': *va_arg(*args, double *)=d; break;
	}
	return 1;
}
static int icvt_s(FILE *f, va_list *args, int store, int width, int type){
	int c, nn, n;
	char *s, b[MB_LEN_MAX+1];
	wchar_t *ws;

	s = 0;
	ws = 0;
	if(store){
		if(type=='l') ws=va_arg(*args, wchar_t *);
		else s=va_arg(*args, char *);
	}
	do
		c=ngetc(f);
	while(isspace(c));
	if(width--==0){
		nungetc(c, f);
		goto Done;
	}
	nn=0;
	while(!isspace(c)){
		if(c==EOF){
			nread--;
			if(nn==0) return 0;
			else goto Done;
		}
		nn++;
		n=widemode || type=='l'? getmb(f, c, b): (b[0]=c, 1);
		if(!putmb(&s, &ws, b, n)) return 0;
		wgetc(c, f, Done);
	}
	nungetc(c, f);
Done:
	if(s) *s='\0';
	if(ws) *ws=0;
	return 1;
}
static int icvt_c(FILE *f, va_list *args, int store, int width, int type){
	int c, n;
	char *s, b[MB_LEN_MAX+1];
	wchar_t *ws;

	s = 0;
	ws = 0;
	if(store){
		if(type=='l') ws=va_arg(*args, wchar_t *);
		else s=va_arg(*args, char *);
	}
	if(width<0) width=1;
	for(;;){
		wgetc(c, f, Done);
		if(c==EOF) return 0;
		n=widemode || type=='l'? getmb(f, c, b): (b[0]=c, 1);
		if(!putmb(&s, &ws, b, n)) return 0;
	}
Done:
	return 1;
}
/* the next character of the pattern, a rune if runes, and in *n how many bytes it is */
static int patc(const char *p, int runes, int *n){
	wchar_t w;
	int k;
	*n=1;
	if(!runes) return *(unsigned char *)p;
	k=mbtowc(&w, p, fmtp-p);
	if(k<=0) return *(unsigned char *)p;
	*n=k;
	return w;
}
static int match(int c, const char *pat, int runes){
	int ok=1, a, z, n, m;
	if(*pat=='^'){
		ok=!ok;
		pat++;
	}
	while(pat<fmtp){
		a=patc(pat, runes, &n);
		if(pat+n+1<fmtp && pat[n]=='-'){
			z=patc(pat+n+1, runes, &m);
			if(a<=c && c<=z || z<=c && c<=a)
				return ok;
			pat+=n+1+m;
			continue;
		}
		if(c==a) return ok;
		pat+=n;
	}
	return !ok;
}
static int icvt_sq(FILE *f, va_list *args, int store, int width, int type){
	int c, nn, n, whole;
	char *s, b[MB_LEN_MAX+1];
	wchar_t *ws, w;
	const char *pat;

	s = 0;
	ws = 0;
	whole = widemode || type=='l';
	pat=++fmtp;
	if(*fmtp=='^') fmtp++;
	if(*fmtp!='\0') fmtp++;
	while(*fmtp!='\0' && *fmtp!=']') fmtp++;
	if(store){
		if(type=='l') ws=va_arg(*args, wchar_t *);
		else s=va_arg(*args, char *);
	}
	nn=0;
	for(;;){
		wgetc(c, f, Done);
		if(c==EOF){
			nread--;
			if(nn==0) return 0;
			else goto Done;
		}
		if(whole){
			n=getmb(f, c, b);
			if(mbtowc(&w, b, n)<0 || !match(w, pat, 1)){
				while(n>0) nungetc((unsigned char)b[--n], f);
				goto Done;
			}
		}else{
			b[0]=c;
			n=1;
			if(!match(c, pat, 0)) break;
		}
		putmb(&s, &ws, b, n);
		nn++;
	}
	nungetc(c, f);
Done:
	if(nn==0) return 0;
	if(s) *s='\0';
	if(ws) *ws=0;
	return 1;
}
