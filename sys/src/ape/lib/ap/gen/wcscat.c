#include <wchar.h>

wchar_t*
wcscat(wchar_t *d, const wchar_t *s)
{
	wcscpy(d + wcslen(d), s);
	return d;
}
