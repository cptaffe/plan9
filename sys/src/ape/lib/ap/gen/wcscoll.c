#include <wchar.h>

/* the C locale's order is the runes' */
int
wcscoll(const wchar_t *a, const wchar_t *b)
{
	return wcscmp(a, b);
}

size_t
wcsxfrm(wchar_t *d, const wchar_t *s, size_t n)
{
	size_t l;

	l = wcslen(s);
	if(l < n)
		wcscpy(d, s);
	return l;
}
