#include <wchar.h>
#include <errno.h>
#include <string.h>
#include <utf.h>

/*
 * C95's restartable conversions.  The multibyte form is UTF-8,
 * as libutf's chartorune and runetochar have it: a sequence too
 * long, a surrogate or a rune past Runemax is an error, EILSEQ.
 * A state holds the bytes of a rune not yet whole.
 */
static mbstate_t mbrlenstate, mbrtowcstate, wcrtombstate, mbsrstate, wcsrstate;

static size_t
bad(mbstate_t *ps)
{
	ps->__n = 0;
	errno = EILSEQ;
	return (size_t)-1;
}

/*
 * whether b[0..n) can begin a rune: each byte after the first is
 * 10xxxxxx, and the second is in the range the first allows (no
 * overlong form, no surrogate, nothing past Runemax)
 */
static int
prefix(char *b, int n)
{
	int c, lo, hi, i;

	c = b[0] & 0xFF;
	if(c < 0x80)
		return 1;
	if(c < 0xC2 || c > 0xF4)
		return 0;
	lo = 0x80;
	hi = 0xBF;
	if(c == 0xE0)
		lo = 0xA0;
	else if(c == 0xED)
		hi = 0x9F;
	else if(c == 0xF0)
		lo = 0x90;
	else if(c == 0xF4)
		hi = 0x8F;
	if(n > 1 && ((b[1] & 0xFF) < lo || (b[1] & 0xFF) > hi))
		return 0;
	for(i = 2; i < n; i++)
		if((b[i] & 0xC0) != 0x80)
			return 0;
	return 1;
}

size_t
mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	char buf[UTFmax+1];
	int k, m, old;
	Rune r;

	if(ps == 0)
		ps = &mbrtowcstate;
	if(s == 0){
		if(ps->__n != 0)
			return bad(ps);
		return 0;
	}
	if(n == 0)
		return (size_t)-2;
	/* a byte at a time: not past the rune, which may end the string */
	memset(buf, 0, sizeof buf);
	memmove(buf, ps->__b, ps->__n);
	old = ps->__n;
	m = old;
	do{
		buf[m] = s[m-old];
		m++;
		if(!prefix(buf, m))
			return bad(ps);
	}while(!fullrune(buf, m) && m-old < n);
	if(!fullrune(buf, m)){
		memmove(ps->__b, buf, m);
		ps->__n = m;
		return (size_t)-2;
	}
	k = chartorune(&r, buf);
	ps->__n = 0;
	if(r == Runeerror && memcmp(buf, "\xEF\xBF\xBD", 3) != 0)
		return bad(ps);
	if(pwc)
		*pwc = r;
	return r == 0? 0: k - old;
}

size_t
mbrlen(const char *s, size_t n, mbstate_t *ps)
{
	return mbrtowc(0, s, n, ps? ps: &mbrlenstate);
}

int
mbsinit(const mbstate_t *ps)
{
	return ps == 0 || ps->__n == 0 && ps->__c == 0;
}

wint_t
btowc(int c)
{
	if(c < 0 || c >= Runeself)
		return WEOF;
	return c;
}

int
wctob(wint_t c)
{
	if(c >= Runeself)
		return EOF;
	return c;
}

size_t
wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	char buf[UTFmax];
	Rune r;

	if(ps == 0)
		ps = &wcrtombstate;
	if(s == 0)
		s = buf, wc = 0;
	if(wc < 0 || wc > Runemax || (wc >= 0xD800 && wc <= 0xDFFF))
		return bad(ps);
	ps->__n = 0;
	r = wc;
	return runetochar(s, &r);
}

size_t
mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps)
{
	const char *s;
	size_t i, k, m;
	wchar_t c;

	if(ps == 0)
		ps = &mbsrstate;
	s = *src;
	for(i = 0; dst == 0 || i < len; i++){
		for(m = 0; m < UTFmax && s[m]; m++)	/* not past the NUL */
			;
		k = mbrtowc(&c, s, m < UTFmax? m+1: m, ps);
		if(k == (size_t)-1 || k == (size_t)-2){
			if(dst)
				*src = s;
			return bad(ps);
		}
		if(dst)
			dst[i] = c;
		if(c == 0){
			if(dst)
				*src = 0;
			return i;
		}
		s += k;
	}
	*src = s;
	return i;
}

size_t
wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps)
{
	char buf[UTFmax];
	const wchar_t *w;
	size_t n, k;

	if(ps == 0)
		ps = &wcsrstate;
	w = *src;
	for(n = 0;; w++){
		k = wcrtomb(buf, *w, ps);
		if(k == (size_t)-1){
			if(dst)
				*src = w;
			return k;
		}
		if(*w == 0){
			if(dst){
				if(n < len)
					dst[n] = 0;
				else
					break;
				*src = 0;
			}
			return n;
		}
		if(dst){
			if(n+k > len)
				break;
			memmove(dst+n, buf, k);
		}
		n += k;
	}
	*src = w;
	return n;
}
