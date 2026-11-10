/*
 * C95 stdio -- fputwc, putwc, putwchar, fputws
 */
#include "iolib.h"
#include <wchar.h>
#include <limits.h>
#include <string.h>

wint_t
fputwc(wchar_t c, FILE *f)
{
	char b[MB_LEN_MAX];
	mbstate_t st;
	int n;

	fwide(f, 1);
	memset(&st, 0, sizeof st);
	n = wcrtomb(b, c, &st);
	if(n < 0){
		f->state = ERR;
		return WEOF;
	}
	if(fwrite(b, 1, n, f) != n)
		return WEOF;
	return c;
}

wint_t
putwc(wchar_t c, FILE *f)
{
	return fputwc(c, f);
}

wint_t
putwchar(wchar_t c)
{
	return fputwc(c, stdout);
}

int
fputws(const wchar_t *s, FILE *f)
{
	for(; *s; s++)
		if(fputwc(*s, f) == WEOF)
			return EOF;
	return 0;
}
