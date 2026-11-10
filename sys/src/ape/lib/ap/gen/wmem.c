#include <wchar.h>
#include <string.h>

wchar_t*
wmemcpy(wchar_t *d, const wchar_t *s, size_t n)
{
	return memcpy(d, s, n*sizeof(wchar_t));
}

wchar_t*
wmemmove(wchar_t *d, const wchar_t *s, size_t n)
{
	return memmove(d, s, n*sizeof(wchar_t));
}

wchar_t*
wmemset(wchar_t *d, wchar_t c, size_t n)
{
	size_t i;

	for(i = 0; i < n; i++)
		d[i] = c;
	return d;
}

int
wmemcmp(const wchar_t *a, const wchar_t *b, size_t n)
{
	for(; n > 0; n--, a++, b++)
		if(*a != *b)
			return *a < *b? -1: 1;
	return 0;
}

wchar_t*
wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
	for(; n > 0; n--, s++)
		if(*s == c)
			return (wchar_t*)s;
	return 0;
}
