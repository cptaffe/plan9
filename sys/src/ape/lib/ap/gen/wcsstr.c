#include <wchar.h>

wchar_t*
wcsstr(const wchar_t *s1, const wchar_t *s2)
{
	size_t n;

	n = wcslen(s2);
	for(; *s1; s1++)
		if(wcsncmp(s1, s2, n) == 0)
			return (wchar_t*)s1;
	return n == 0? (wchar_t*)s1: 0;
}
