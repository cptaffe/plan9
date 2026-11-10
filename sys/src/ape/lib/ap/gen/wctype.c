#include <wctype.h>
#include <string.h>
#include <utf.h>

/*
 * C95's classes of wide characters: letters, case and space are
 * libutf's (runetype.c); digits are ASCII's, as C says; the rest
 * follow from those, as ctype's do in the C locale.
 */
static int
valid(wint_t c)
{
	return c <= Runemax && (c < 0xD800 || c > 0xDFFF);
}

int
iswalpha(wint_t c)
{
	return valid(c) && isalpharune(c);
}

int
iswdigit(wint_t c)
{
	return c >= '0' && c <= '9';
}

int
iswalnum(wint_t c)
{
	return iswalpha(c) || iswdigit(c);
}

int
iswxdigit(wint_t c)
{
	return iswdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int
iswspace(wint_t c)
{
	if(c >= '\t' && c <= '\r')	/* libutf's table has only some of them */
		return 1;
	return valid(c) && isspacerune(c);
}

int
iswblank(wint_t c)
{
	if(c == ' ' || c == '\t')
		return 1;
	return c >= 0x80 && c != 0x85 && c != 0x2028 && c != 0x2029 && iswspace(c);
}

int
iswcntrl(wint_t c)
{
	return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == 0x2028 || c == 0x2029;
}

int
iswprint(wint_t c)
{
	return valid(c) && !iswcntrl(c) && (c & 0xFFFE) != 0xFFFE;
}

int
iswgraph(wint_t c)
{
	return iswprint(c) && !iswspace(c);
}

int
iswpunct(wint_t c)
{
	return iswgraph(c) && !iswalnum(c);
}

int
iswlower(wint_t c)
{
	return valid(c) && islowerrune(c);
}

int
iswupper(wint_t c)
{
	return valid(c) && isupperrune(c);
}

wint_t
towlower(wint_t c)
{
	return valid(c)? tolowerrune(c): c;
}

wint_t
towupper(wint_t c)
{
	return valid(c)? toupperrune(c): c;
}

static struct {
	char *name;
	int (*f)(wint_t);
} classes[] = {
	0,	0,
	"alnum",	iswalnum,
	"alpha",	iswalpha,
	"blank",	iswblank,
	"cntrl",	iswcntrl,
	"digit",	iswdigit,
	"graph",	iswgraph,
	"lower",	iswlower,
	"print",	iswprint,
	"punct",	iswpunct,
	"space",	iswspace,
	"upper",	iswupper,
	"xdigit",	iswxdigit,
};

wctype_t
wctype(const char *name)
{
	int i;

	for(i = 1; i < sizeof classes/sizeof classes[0]; i++)
		if(strcmp(name, classes[i].name) == 0)
			return i;
	return 0;
}

int
iswctype(wint_t c, wctype_t t)
{
	if(t <= 0 || t >= sizeof classes/sizeof classes[0])
		return 0;
	return (*classes[t].f)(c);
}

wctrans_t
wctrans(const char *name)
{
	if(strcmp(name, "tolower") == 0)
		return 1;
	if(strcmp(name, "toupper") == 0)
		return 2;
	return 0;
}

wint_t
towctrans(wint_t c, wctrans_t t)
{
	switch(t){
	case 1:
		return towlower(c);
	case 2:
		return towupper(c);
	}
	return c;
}
