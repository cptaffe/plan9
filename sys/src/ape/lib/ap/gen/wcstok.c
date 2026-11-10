#include <wchar.h>

wchar_t*
wcstok(wchar_t *s, const wchar_t *b, wchar_t **last)
{
	wchar_t *os;

	if(s == 0)
		s = *last;
	s += wcsspn(s, b);
	if(*s == 0){
		*last = s;
		return 0;
	}
	os = s;
	s += wcscspn(s, b);
	if(*s)
		*s++ = 0;
	*last = s;
	return os;
}
