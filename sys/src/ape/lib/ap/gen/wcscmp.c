#include <wchar.h>

int
wcscmp(const wchar_t *a, const wchar_t *b)
{
	for(; *a == *b; a++, b++)
		if(*a == 0)
			return 0;
	return *a < *b? -1: 1;
}
