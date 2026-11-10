/*
 * C95 stdio -- wscanf and the rest of its family
 *	The format and a string to scan are made UTF-8; scanf's
 *	conversions run in the mode where a character is read whole
 *	(_vfscanfw).
 */
#include "iolib.h"
#include <wchar.h>
#include <string.h>

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

int
vfwscanf(FILE *f, const wchar_t *fmt, va_list args)
{
	char *nf;
	int n;

	fwide(f, 1);
	nf = narrow(fmt);
	if(nf == 0)
		return EOF;
	n = _vfscanfw(f, nf, args);
	free(nf);
	return n;
}

int
vwscanf(const wchar_t *fmt, va_list args)
{
	return vfwscanf(stdin, fmt, args);
}

int
vswscanf(const wchar_t *ws, const wchar_t *fmt, va_list args)
{
	char *s, *nf;
	FILE *f;
	int n;

	s = narrow(ws);
	nf = narrow(fmt);
	n = EOF;
	if(s && nf && (f = _IO_sopenr(s)) != 0){
		n = _vfscanfw(f, nf, args);
		_IO_sclose(f);
	}
	free(s);
	free(nf);
	return n;
}

int
fwscanf(FILE *f, const wchar_t *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vfwscanf(f, fmt, a);
	va_end(a);
	return n;
}

int
wscanf(const wchar_t *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vfwscanf(stdin, fmt, a);
	va_end(a);
	return n;
}

int
swscanf(const wchar_t *ws, const wchar_t *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vswscanf(ws, fmt, a);
	va_end(a);
	return n;
}
