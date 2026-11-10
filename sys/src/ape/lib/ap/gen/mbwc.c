#include <stdlib.h>
#include <wchar.h>
#include <string.h>

/*
 * The C89 conversions, by C95's restartable ones (mbrwc.c):
 * UTF-8 has no shift states, so each conversion stands alone.
 */
int
mblen(const char *s, size_t n)
{
	return mbtowc(0, s, n);
}

int
mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	mbstate_t st;
	size_t k;

	if(s == 0)
		return 0;
	memset(&st, 0, sizeof st);
	k = mbrtowc(pwc, s, n, &st);
	if(k == (size_t)-1 || k == (size_t)-2)
		return -1;
	return k;
}

int
wctomb(char *s, wchar_t wc)
{
	mbstate_t st;

	if(s == 0)
		return 0;
	memset(&st, 0, sizeof st);
	return wcrtomb(s, wc, &st);
}

size_t
mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	mbstate_t st;

	memset(&st, 0, sizeof st);
	return mbsrtowcs(pwcs, &s, n, &st);
}

size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	mbstate_t st;

	memset(&st, 0, sizeof st);
	return wcsrtombs(s, &pwcs, n, &st);
}
