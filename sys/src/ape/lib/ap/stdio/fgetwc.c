/*
 * C95 stdio -- fgetwc, getwc, getwchar, fgetws, ungetwc
 */
#include "iolib.h"
#include <wchar.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

static wint_t
ufgetwc(FILE *f)
{
	char b[MB_LEN_MAX];
	mbstate_t st;
	wchar_t w;
	size_t k;
	int c, n;

	fwide(f, 1);
	memset(&st, 0, sizeof st);
	for(n = 0; n < MB_LEN_MAX; ){
		c = getc(f);
		if(c == EOF){
			if(n == 0)
				return WEOF;
			break;
		}
		b[n++] = c;
		k = mbrtowc(&w, b+n-1, 1, &st);
		if(k == (size_t)-2)
			continue;
		if(k == (size_t)-1)
			break;
		return w;
	}
	errno = EILSEQ;
	f->state = ERR;
	return WEOF;
}

wint_t
getwc(FILE *f)
{
	return fgetwc(f);
}

wint_t
getwchar(void)
{
	return fgetwc(stdin);
}

static wchar_t *
ufgetws(wchar_t *s, int n, FILE *f)
{
	wint_t c;
	int i;

	for(i = 0; i < n-1; ){
		c = fgetwc(f);
		if(c == WEOF){
			if(i == 0 || ferror(f))
				return 0;
			break;
		}
		s[i++] = c;
		if(c == L'\n')
			break;
	}
	s[i] = 0;
	return s;
}

/* its bytes, pushed back last first */
static wint_t
uungetwc(wint_t c, FILE *f)
{
	char b[MB_LEN_MAX];
	mbstate_t st;
	int n;

	if(c == WEOF)
		return WEOF;
	fwide(f, 1);
	memset(&st, 0, sizeof st);
	n = wcrtomb(b, c, &st);
	if(n < 0)
		return WEOF;
	while(n > 0)
		if(ungetc((unsigned char)b[--n], f) == EOF)
			return WEOF;
	return c;
}

wint_t
fgetwc(FILE *f)
{
	wint_t r;

	flockfile(f);
	r = ufgetwc(f);
	funlockfile(f);
	return r;
}

wchar_t *
fgetws(wchar_t *s, int n, FILE *f)
{
	wchar_t *r;

	flockfile(f);
	r = ufgetws(s, n, f);
	funlockfile(f);
	return r;
}

wint_t
ungetwc(wint_t c, FILE *f)
{
	wint_t r;

	flockfile(f);
	r = uungetwc(c, f);
	funlockfile(f);
	return r;
}
