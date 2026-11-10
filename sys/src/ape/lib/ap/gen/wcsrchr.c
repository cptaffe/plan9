#include <wchar.h>

wchar_t*
wcsrchr(const wchar_t *s, wchar_t c)
{
	const wchar_t *r;

	r = 0;
	for(;; s++){
		if(*s == c)
			r = s;
		if(*s == 0)
			return (wchar_t*)r;
	}
}
