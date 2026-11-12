/*
 * The encoder: values written to a Fmt, with the commas and colons put in between. jsonquote's
 * escapes are 9front's libjson's printstring (MIT); bytes that are not UTF-8 become U+FFFD, as
 * Go writes them.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <json.h>

#define	BIT(a, d)	((a)[(d)>>3] & 1<<((d)&7))
#define	SET(a, d)	((a)[(d)>>3] |= 1<<((d)&7))
#define	CLR(a, d)	((a)[(d)>>3] &= ~(1<<((d)&7)))

void
jsonenc(Jsonenc *e, Fmt *f)
{
	memset(e, 0, sizeof *e);
	e->f = f;
	SET(e->first, 0);
}

static int
check(Jsonenc *e, int n)
{
	if(n < 0)
		e->err = 1;
	return e->err ? -1 : 0;
}

/* sep puts the comma before a value or key, but the first of its container's, or one after a key. */
static int
sep(Jsonenc *e)
{
	if(e->err)
		return -1;
	if(e->afterkey){
		e->afterkey = 0;
		return 0;
	}
	if(BIT(e->first, e->depth)){
		CLR(e->first, e->depth);
		return 0;
	}
	if(e->depth == 0)
		return check(e, fmtrune(e->f, '\n'));	/* values one after another, as Go's Encoder has them */
	return check(e, fmtrune(e->f, ','));
}

/* jsonbegin opens an object ('{') or an array ('['). */
int
jsonbegin(Jsonenc *e, int c)
{
	if(sep(e) < 0)
		return -1;
	if(e->depth+1 >= Jsondepth){
		werrstr("json: too deep");
		return e->err = -1;
	}
	e->depth++;
	SET(e->first, e->depth);
	if(c == '{')
		SET(e->isobj, e->depth);
	else
		CLR(e->isobj, e->depth);
	return check(e, fmtrune(e->f, c));
}

/* jsonend closes what jsonbegin opened last. */
int
jsonend(Jsonenc *e)
{
	int c;

	if(e->err)
		return -1;
	if(e->depth == 0){
		werrstr("json: nothing to end");
		return e->err = -1;
	}
	c = BIT(e->isobj, e->depth) ? '}' : ']';
	e->depth--;
	return check(e, fmtrune(e->f, c));
}

int
jsonkey(Jsonenc *e, char *k)
{
	if(sep(e) < 0)
		return -1;
	if(check(e, jsonquote(e->f, k, strlen(k))) < 0 || check(e, fmtrune(e->f, ':')) < 0)
		return -1;
	e->afterkey = 1;
	return 0;
}

int
jsonstring(Jsonenc *e, char *s)
{
	return jsonstringn(e, s, strlen(s));
}

int
jsonstringn(Jsonenc *e, char *s, long n)
{
	if(sep(e) < 0)
		return -1;
	return check(e, jsonquote(e->f, s, n));
}

/* jsonnumber writes a number's text, as it is. */
int
jsonnumber(Jsonenc *e, char *s)
{
	if(sep(e) < 0)
		return -1;
	return check(e, fmtstrcpy(e->f, s));
}

int
jsonint(Jsonenc *e, vlong v)
{
	if(sep(e) < 0)
		return -1;
	return check(e, fmtprint(e->f, "%lld", v));
}

int
jsonbool(Jsonenc *e, int b)
{
	if(sep(e) < 0)
		return -1;
	return check(e, fmtstrcpy(e->f, b ? "true" : "false"));
}

int
jsonnull(Jsonenc *e)
{
	if(sep(e) < 0)
		return -1;
	return check(e, fmtstrcpy(e->f, "null"));
}

/* jsonrawval writes a value's text, as it is: a Jsonraw's, given back byte for byte (if it is UTF-8). */
int
jsonrawval(Jsonenc *e, char *s, long n)
{
	if(sep(e) < 0)
		return -1;
	/* %.*s's precision counts runes */
	return check(e, fmtprint(e->f, "%.*s", utfnlen(s, n), s));
}

/* jsonquote writes the n bytes at s as a JSON string. */
int
jsonquote(Fmt *f, char *s, long n)
{
	char *es, b[UTFmax+1];
	Rune r;
	int m, k, bad;

	k = fmtrune(f, '"');
	for(es = s+n; s < es; s += m){
		bad = 0;
		if((uchar)*s < Runeself){
			r = *s;
			m = 1;
		}else{
			/* a copy, so that a rune cut short at the end reads as an error, not past it */
			memset(b, 0, sizeof b);
			memmove(b, s, es-s < UTFmax ? es-s : UTFmax);
			m = chartorune(&r, b);
			bad = r == Runeerror && m == 1;
		}
		switch(r){
		case '\\':	k |= fmtstrcpy(f, "\\\\");	break;
		case '\f':	k |= fmtstrcpy(f, "\\f");	break;
		case '\b':	k |= fmtstrcpy(f, "\\b");	break;
		case '\t':	k |= fmtstrcpy(f, "\\t");	break;
		case '\n':	k |= fmtstrcpy(f, "\\n");	break;
		case '\r':	k |= fmtstrcpy(f, "\\r");	break;
		case '"':	k |= fmtstrcpy(f, "\\\"");	break;
		default:
			if(r < 0x20)
				k |= fmtprint(f, "\\u%04x", r);
			else if(bad)
				k |= fmtstrcpy(f, "\\ufffd");
			else
				k |= fmtrune(f, r);
		}
	}
	k |= fmtrune(f, '"');
	return k < 0 ? -1 : 0;
}
