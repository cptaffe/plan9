#include <string.h>

typedef unsigned char uchar;

int
strncasecmp(const char *s1, const char *s2, size_t n)
{
	int c1, c2;

	for(; n > 0 && *s1; n--){
		c1 = *(uchar*)s1++;
		c2 = *(uchar*)s2++;

		if(c1 == c2)
			continue;

		if(c1 >= 'A' && c1 <= 'Z')
			c1 -= 'A' - 'a';

		if(c2 >= 'A' && c2 <= 'Z')
			c2 -= 'A' - 'a';

		if(c1 != c2)
			return c1 - c2;
	}
	if(n == 0)
		return 0;
	return -*(uchar*)s2;
}

int
strncasecmp_l(const char *s1, const char *s2, size_t n, struct __locale *l)
{
	return strncasecmp(s1, s2, n);
}
