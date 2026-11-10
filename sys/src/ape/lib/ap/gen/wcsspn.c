#include <wchar.h>

size_t
wcsspn(const wchar_t *s, const wchar_t *b)
{
	const wchar_t *p;

	for(p = s; *p && wcschr(b, *p); p++)
		;
	return p - s;
}

size_t
wcscspn(const wchar_t *s, const wchar_t *b)
{
	const wchar_t *p;

	for(p = s; *p && !wcschr(b, *p); p++)
		;
	return p - s;
}

wchar_t*
wcspbrk(const wchar_t *s, const wchar_t *b)
{
	s += wcscspn(s, b);
	return *s? (wchar_t*)s: 0;
}
