#include <wchar.h>

wchar_t*
wcschr(const wchar_t *s, wchar_t c)
{
	for(;; s++){
		if(*s == c)
			return (wchar_t*)s;
		if(*s == 0)
			return 0;
	}
}
