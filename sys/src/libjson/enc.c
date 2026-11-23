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

static int
bad(Jsonenc *e, char *why)
{
	werrstr("json: %s", why);
	return e->err = -1;
}

static int
inobj(Jsonenc *e)
{
	return e->depth > 0 && BIT(e->isobj, e->depth);
}

static int
comma(Jsonenc *e)
{
	if(e->afterkey){
		e->afterkey = 0;
		return 0;
	}
	if(BIT(e->first, e->depth)){
		CLR(e->first, e->depth);
		return 0;
	}
	if(e->depth == 0)
		return check(e, fmtrune(e->f, '\n'));
	return check(e, fmtrune(e->f, ','));
}

static int
sep(Jsonenc *e)
{
	if(e->err)
		return -1;
	if(inobj(e) && !e->afterkey)
		return bad(e, "a value with no key");
	return comma(e);
}

int
jsonbegin(Jsonenc *e, int c)
{
	if(e->err)
		return -1;
	if(c != '{' && c != '[')
		return bad(e, "begin of neither { nor [");
	if(sep(e) < 0)
		return -1;
	if(e->depth >= Jsondepth)
		return bad(e, "too deep");
	e->depth++;
	SET(e->first, e->depth);
	if(c == '{')
		SET(e->isobj, e->depth);
	else
		CLR(e->isobj, e->depth);
	return check(e, fmtrune(e->f, c));
}

int
jsonend(Jsonenc *e)
{
	int c;

	if(e->err)
		return -1;
	if(e->depth == 0)
		return bad(e, "nothing to end");
	if(e->afterkey)
		return bad(e, "a key with no value");
	c = BIT(e->isobj, e->depth) ? '}' : ']';
	e->depth--;
	return check(e, fmtrune(e->f, c));
}

int
jsonkey(Jsonenc *e, char *k)
{
	if(e->err)
		return -1;
	if(!inobj(e))
		return bad(e, "a key outside an object");
	if(e->afterkey)
		return bad(e, "a key after a key");
	if(comma(e) < 0)
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

int
jsonrawval(Jsonenc *e, char *s, long n)
{
	if(sep(e) < 0)
		return -1;
	return check(e, fmtprint(e->f, "%.*s", utfnlen(s, n), s));
}

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
