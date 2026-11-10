#include <inttypes.h>
#include <stdlib.h>

intmax_t
imaxabs(intmax_t a)
{
	if(a < 0)
		return -a;
	return a;
}

imaxdiv_t
imaxdiv(intmax_t numer, intmax_t denom)
{
	imaxdiv_t ans;

	ans.quot = numer/denom;
	ans.rem = numer-ans.quot*denom;
	return ans;
}

intmax_t
strtoimax(const char *s, char **ep, int base)
{
	return strtoll(s, ep, base);
}

uintmax_t
strtoumax(const char *s, char **ep, int base)
{
	return strtoull(s, ep, base);
}
