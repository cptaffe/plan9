#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

/*
 * The wide strtol and strtod: a number is ASCII, so each is the
 * narrow one's, on the ASCII after the space.
 */
static char*
narrow(const wchar_t *s, const wchar_t **start)
{
	const wchar_t *p;
	char *b;
	int i;

	while(iswspace(*s))
		s++;
	for(p = s; *p > 0 && *p < 0x80; p++)
		;
	b = malloc(p-s+1);
	if(b == 0)
		return 0;
	for(i = 0; s+i < p; i++)
		b[i] = s[i];
	b[i] = 0;
	*start = s;
	return b;
}

static void
end(const wchar_t *s, const wchar_t *start, char *b, char *e, wchar_t **ep)
{
	if(ep)
		*ep = (wchar_t*)(e == b? s: start + (e-b));
	free(b);
}

#define	WCSTO(name, type, call)	\
type \
name(const wchar_t *s, wchar_t **ep, int base) \
{ \
	const wchar_t *start; \
	char *b, *e; \
	type v; \
\
	b = narrow(s, &start); \
	if(b == 0){ \
		if(ep) \
			*ep = (wchar_t*)s; \
		return 0; \
	} \
	v = call(b, &e, base); \
	end(s, start, b, e, ep); \
	return v; \
}

WCSTO(wcstol, long, strtol)
WCSTO(wcstoll, long long, strtoll)
WCSTO(wcstoul, unsigned long, strtoul)
WCSTO(wcstoull, unsigned long long, strtoull)
WCSTO(wcstoimax, intmax_t, strtoll)
WCSTO(wcstoumax, uintmax_t, strtoull)

double
wcstod(const wchar_t *s, wchar_t **ep)
{
	const wchar_t *start;
	char *b, *e;
	double v;

	b = narrow(s, &start);
	if(b == 0){
		if(ep)
			*ep = (wchar_t*)s;
		return 0;
	}
	v = strtod(b, &e);
	end(s, start, b, e, ep);
	return v;
}

float
wcstof(const wchar_t *s, wchar_t **ep)
{
	const wchar_t *start;
	char *b, *e;
	float v;

	b = narrow(s, &start);
	if(b == 0){
		if(ep)
			*ep = (wchar_t*)s;
		return 0;
	}
	v = strtof(b, &e);
	end(s, start, b, e, ep);
	return v;
}

long double
wcstold(const wchar_t *s, wchar_t **ep)
{
	return wcstod(s, ep);
}
