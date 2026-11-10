#include <wchar.h>

wchar_t*
wcsncpy(wchar_t *d, const wchar_t *s, size_t n)
{
	size_t i;

	for(i = 0; i < n && s[i]; i++)
		d[i] = s[i];
	for(; i < n; i++)
		d[i] = 0;
	return d;
}
