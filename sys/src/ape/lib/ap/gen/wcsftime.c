#include <wchar.h>
#include <time.h>
#include <stdlib.h>
#include <utf.h>

/*
 * strftime's, in UTF-8, made wide
 */
size_t
wcsftime(wchar_t *s, size_t n, const wchar_t *fmt, const struct tm *tm)
{
	const wchar_t *f;
	char *nf, *b;
	size_t k, r;

	f = fmt;
	k = wcsrtombs(0, &f, 0, 0);
	if(k == (size_t)-1)
		return 0;
	nf = malloc(k+1);
	b = malloc(n*UTFmax+1);
	r = 0;
	if(nf && b){
		f = fmt;
		wcsrtombs(nf, &f, k+1, 0);
		if(strftime(b, n*UTFmax+1, nf, tm) > 0 || nf[0] == 0){
			r = mbstowcs(s, b, n);
			if(r >= n)
				r = 0;
		}
	}
	free(nf);
	free(b);
	return r;
}
