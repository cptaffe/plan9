#include <wchar.h>

wchar_t*
wcscpy(wchar_t *d, const wchar_t *s)
{
	wchar_t *p;

	p = d;
	while((*p++ = *s++) != 0)
		;
	return d;
}
