#include <uchar.h>
#include <wchar.h>
#include <errno.h>

/*
 * C11's UTF-16 and UTF-32, by the wide conversions (mbrwc.c): a
 * char32_t is a wchar_t.  A state holds the low surrogate mbrtoc16
 * has yet to give, or the high one c16rtomb has had.
 */
static mbstate_t c16state, mb16state, c32state, mb32state;

size_t
mbrtoc32(char32_t *pc, const char *s, size_t n, mbstate_t *ps)
{
	wchar_t w;
	size_t k;

	k = mbrtowc(&w, s, n, ps? ps: &mb32state);
	if(pc && k <= n)
		*pc = w;
	return k;
}

size_t
c32rtomb(char *s, char32_t c, mbstate_t *ps)
{
	return wcrtomb(s, c, ps? ps: &c32state);
}

size_t
mbrtoc16(char16_t *pc, const char *s, size_t n, mbstate_t *ps)
{
	wchar_t w;
	size_t k;

	if(ps == 0)
		ps = &mb16state;
	if(ps->__c){
		if(pc)
			*pc = ps->__c;
		ps->__c = 0;
		return (size_t)-3;
	}
	if(s == 0)
		return mbrtowc(0, 0, 0, ps);
	k = mbrtowc(&w, s, n, ps);
	if(k > n)
		return k;
	if(w >= 0x10000){
		w -= 0x10000;
		ps->__c = 0xDC00 | (w & 0x3FF);
		w = 0xD800 | (w >> 10);
	}
	if(pc)
		*pc = w;
	return k;
}

size_t
c16rtomb(char *s, char16_t c, mbstate_t *ps)
{
	wchar_t w;

	if(ps == 0)
		ps = &c16state;
	if(s == 0){
		if(ps->__c)
			goto bad;	/* a high surrogate, with no low one */
		return 1;
	}
	if(c >= 0xD800 && c <= 0xDBFF){
		if(ps->__c)
			goto bad;
		ps->__c = c;
		return 0;
	}
	if(c >= 0xDC00 && c <= 0xDFFF){
		if(ps->__c == 0)
			goto bad;
		w = 0x10000 + ((ps->__c - 0xD800) << 10) + (c - 0xDC00);
		ps->__c = 0;
		return wcrtomb(s, w, ps);
	}
	if(ps->__c)
		goto bad;
	return wcrtomb(s, c, ps);
bad:
	ps->__c = 0;
	errno = EILSEQ;
	return (size_t)-1;
}
