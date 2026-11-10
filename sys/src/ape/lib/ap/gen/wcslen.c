#include <wchar.h>

size_t
wcslen(const wchar_t *s)
{
	const wchar_t *p;

	for(p = s; *p; p++)
		;
	return p - s;
}
