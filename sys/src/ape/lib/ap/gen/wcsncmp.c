#include <wchar.h>

int
wcsncmp(const wchar_t *a, const wchar_t *b, size_t n)
{
	for(; n > 0; n--, a++, b++){
		if(*a != *b)
			return *a < *b? -1: 1;
		if(*a == 0)
			break;
	}
	return 0;
}
