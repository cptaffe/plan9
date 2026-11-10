/*
 * C95 stdio -- wprintf and the rest of its family
 *	The format is made UTF-8, and printf's conversions run in
 *	the mode where widths count characters (_vfprintfw).  They
 *	write to a string, which counts the characters and makes
 *	them wide, or which goes to the stream.
 */
#include "iolib.h"
#include <wchar.h>
#include <string.h>
#include <utf.h>

static char*
narrow(const wchar_t *w)
{
	const wchar_t *p;
	size_t n;
	char *s;

	p = w;
	n = wcsrtombs(0, &p, 0, 0);
	if(n == (size_t)-1)
		return 0;
	s = malloc(n+1);
	if(s == 0)
		return 0;
	p = w;
	wcsrtombs(s, &p, n+1, 0);
	return s;
}

/* the output, in UTF-8, with *np its characters */
static char*
format(const wchar_t *fmt, va_list args, int *np)
{
	char *nf, *s;
	FILE *f;
	int n;

	nf = narrow(fmt);
	if(nf == 0)
		return 0;
	f = _IO_sopenw();
	if(f == 0){
		free(nf);
		return 0;
	}
	n = _vfprintfw(f, nf, args);
	s = _IO_sclose(f);
	free(nf);
	if(n < 0){
		free(s);
		return 0;
	}
	*np = utflen(s);
	return s;
}

int
vfwprintf(FILE *f, const wchar_t *fmt, va_list args)
{
	char *s;
	int n;

	fwide(f, 1);
	s = format(fmt, args, &n);
	if(s == 0)
		return -1;
	if(fputs(s, f) == EOF)
		n = -1;
	free(s);
	return n;
}

int
vwprintf(const wchar_t *fmt, va_list args)
{
	return vfwprintf(stdout, fmt, args);
}

/* C99: -1 if the n wide characters, with the NUL, do not hold it */
int
vswprintf(wchar_t *ws, size_t n, const wchar_t *fmt, va_list args)
{
	char *s;
	int k;

	s = format(fmt, args, &k);
	if(s == 0)
		return -1;
	if(k >= n){
		if(n > 0)
			ws[0] = 0;
		free(s);
		return -1;
	}
	mbstowcs(ws, s, n);
	free(s);
	return k;
}

int
fwprintf(FILE *f, const wchar_t *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vfwprintf(f, fmt, a);
	va_end(a);
	return n;
}

int
wprintf(const wchar_t *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vfwprintf(stdout, fmt, a);
	va_end(a);
	return n;
}

int
swprintf(wchar_t *ws, size_t n, const wchar_t *fmt, ...)
{
	va_list a;
	int k;

	va_start(a, fmt);
	k = vswprintf(ws, n, fmt, a);
	va_end(a);
	return k;
}
