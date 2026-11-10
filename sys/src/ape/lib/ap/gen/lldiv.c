#include <stdlib.h>

lldiv_t lldiv(long long numer, long long denom)
{
	lldiv_t ans;
	ans.quot=numer/denom;
	ans.rem=numer-ans.quot*denom;
	return ans;
}
