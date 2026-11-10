#include <wchar.h>

wchar_t*
wcsncat(wchar_t *d, const wchar_t *s, size_t n)
{
	wchar_t *p;

	p = d + wcslen(d);
	while(n-- > 0 && *s)
		*p++ = *s++;
	*p = 0;
	return d;
}
