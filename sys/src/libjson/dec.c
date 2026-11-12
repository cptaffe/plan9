/*
 * The decoder: a stream's tokens, as Go's json.Decoder gives them (Token, More), which keeps
 * the commas and colons to itself. The escapes in strings are 9front's libjson's (MIT).
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <json.h>
#include "impl.h"

enum
{
	Ttop,	/* a value, or the end */
	Tarrstart,	/* after '[': a value or ']' */
	Tarrval,	/* after an array's value: ',' or ']' */
	Tarrcomma,	/* after an array's ',': a value */
	Tobjstart,	/* after '{': a key or '}' */
	Tobjkey,	/* after a key: ':' */
	Tobjcolon,	/* after ':': a value */
	Tobjval,	/* after an object's value: ',' or '}' */
	Tobjcomma,	/* after an object's ',': a key */
};

static Jsondec*
newdec(void)
{
	Jsondec *d;

	d = mallocz(sizeof *d, 1);
	if(d == nil)
		return nil;
	d->peek = -1;
	d->state = Ttop;
	return d;
}

/* jsondec decodes what b reads. */
Jsondec*
jsondec(Biobuf *b)
{
	Jsondec *d;

	if((d = newdec()) != nil)
		d->b = b;
	return d;
}

/* jsondecmem decodes the n bytes at s. */
Jsondec*
jsondecmem(char *s, long n)
{
	Jsondec *d;

	if((d = newdec()) != nil){
		d->p = (uchar*)s;
		d->ep = (uchar*)s+n;
	}
	return d;
}

void
jsondecfree(Jsondec *d)
{
	if(d == nil)
		return;
	free(d->buf);
	free(d->stk);
	free(d->rec);
	free(d);
}

int
_jsonerr(Jsondec *d, char *fmt, ...)
{
	char buf[ERRMAX];
	va_list arg;

	if(d->err[0] == 0){
		va_start(arg, fmt);
		vsnprint(buf, sizeof buf, fmt, arg);
		va_end(arg);
		snprint(d->err, sizeof d->err, "json: offset %lld: %s", d->off, buf);
	}
	werrstr("%s", d->err);
	return -1;
}

static int
grow(char **p, long *a, long n)
{
	char *q;
	long m;

	if(n <= *a)
		return 0;
	m = *a ? *a : 64;
	while(m < n)
		m *= 2;
	if((q = realloc(*p, m)) == nil)
		return -1;
	*p = q;
	*a = m;
	return 0;
}

/* peekc is the next byte, or -1 at the end; getc takes it, into a raw value's text if one is read. */
static int
peekc(Jsondec *d)
{
	if(d->peek < 0){
		if(d->b != nil)
			d->peek = Bgetc(d->b);
		else if(d->p < d->ep)
			d->peek = *d->p++;
	}
	return d->peek;
}

static int
getc(Jsondec *d)
{
	int c;

	c = peekc(d);
	if(c < 0)
		return -1;
	d->peek = -1;
	d->off++;
	if(d->recording){
		if(grow(&d->rec, &d->arec, d->nrec+2) < 0)
			return _jsonerr(d, "out of memory");
		d->rec[d->nrec++] = c;
	}
	return c;
}

static int
peeknonspace(Jsondec *d)
{
	int c;

	for(;;){
		c = peekc(d);
		if(c != ' ' && c != '\t' && c != '\n' && c != '\r')
			return c;
		getc(d);
	}
}

/* put adds b's n bytes to the token's text. */
static int
put(Jsondec *d, long *n, char *b, int m)
{
	if(grow(&d->buf, &d->nbuf, *n+m+1) < 0)
		return _jsonerr(d, "out of memory");
	memmove(d->buf+*n, b, m);
	*n += m;
	d->buf[*n] = 0;
	return 0;
}

static int
hex4(Jsondec *d, Rune *r)
{
	int i, c;

	*r = 0;
	for(i = 0; i < 4; i++){
		c = getc(d);
		if(c >= '0' && c <= '9')
			c -= '0';
		else if(c >= 'a' && c <= 'f')
			c -= 'a' - 10;
		else if(c >= 'A' && c <= 'F')
			c -= 'A' - 10;
		else
			return _jsonerr(d, "bad \\u escape");
		*r = *r*16 + c;
	}
	return 0;
}

/* rune adds r to the token's text. */
static int
rune(Jsondec *d, long *n, Rune r)
{
	char b[UTFmax];

	return put(d, n, b, runetochar(b, &r));
}

/*
 * str reads a string, its quote first, into the token's text. A surrogate pair's escapes are
 * one rune; a surrogate alone is U+FFFD, as Go has it.
 */
static int
str(Jsondec *d, Jsontok *t)
{
	int c, esc;
	long n;
	Rune r, hi;
	char ch;

	getc(d);
	n = 0;
	if(put(d, &n, "", 0) < 0)
		return -1;
	hi = 0;
	for(;;){
		c = getc(d);
		if(c < 0)
			return _jsonerr(d, "unterminated string");
		r = c;
		esc = c == '\\';
		if(esc){
			switch(c = getc(d)){
			case 'n':	r = '\n';	break;
			case 'r':	r = '\r';	break;
			case 't':	r = '\t';	break;
			case 'f':	r = '\f';	break;
			case 'b':	r = '\b';	break;
			case '"': case '/': case '\\':	r = c;	break;
			case 'u':
				if(hex4(d, &r) < 0)
					return -1;
				break;
			default:
				return _jsonerr(d, "bad escape \\%c", c);
			}
		}
		if(hi != 0){
			if(esc && r >= 0xDC00 && r <= 0xDFFF){
				r = 0x10000 + ((hi - 0xD800)<<10 | (r - 0xDC00));
				hi = 0;
				if(rune(d, &n, r) < 0)
					return -1;
				continue;
			}
			hi = 0;
			if(rune(d, &n, Runeerror) < 0)
				return -1;
		}
		if(!esc){
			if(c == '"')
				break;
			if(c < ' ')
				return _jsonerr(d, "control character %#x in string", c);
			ch = c;
			if(put(d, &n, &ch, 1) < 0)
				return -1;
			continue;
		}
		if(r >= 0xD800 && r <= 0xDBFF){
			hi = r;
			continue;
		}
		if(r >= 0xDC00 && r <= 0xDFFF)
			r = Runeerror;
		if(rune(d, &n, r) < 0)
			return -1;
	}
	t->type = JSONstring;
	t->s = d->buf;
	t->n = n;
	return 0;
}

static int
isdigit(int c)
{
	return c >= '0' && c <= '9';
}

/* digits takes the digits that come, at least one. */
static int
digits(Jsondec *d, long *n)
{
	char ch;
	int any;

	for(any = 0; isdigit(peekc(d)); any++){
		ch = getc(d);
		if(put(d, n, &ch, 1) < 0)
			return -1;
	}
	if(!any)
		return _jsonerr(d, "bad number");
	return 0;
}

/* num reads a number, as JSON's grammar has it, and keeps its text. */
static int
num(Jsondec *d, Jsontok *t)
{
	long n;
	char ch;

	n = 0;
	if(put(d, &n, "", 0) < 0)
		return -1;
	if(peekc(d) == '-'){
		ch = getc(d);
		put(d, &n, &ch, 1);
	}
	if(peekc(d) == '0'){
		ch = getc(d);
		if(put(d, &n, &ch, 1) < 0)
			return -1;
	}else if(digits(d, &n) < 0)
		return -1;
	if(peekc(d) == '.'){
		ch = getc(d);
		if(put(d, &n, &ch, 1) < 0 || digits(d, &n) < 0)
			return -1;
	}
	if(peekc(d) == 'e' || peekc(d) == 'E'){
		ch = getc(d);
		if(put(d, &n, &ch, 1) < 0)
			return -1;
		if(peekc(d) == '+' || peekc(d) == '-'){
			ch = getc(d);
			if(put(d, &n, &ch, 1) < 0)
				return -1;
		}
		if(digits(d, &n) < 0)
			return -1;
	}
	t->type = JSONnumber;
	t->s = d->buf;
	t->n = n;
	return 0;
}

static int
lit(Jsondec *d, Jsontok *t)
{
	char w[8];
	int n, c;

	for(n = 0; (c = peekc(d)) >= 'a' && c <= 'z'; n++){
		if(n == sizeof w - 1)
			return _jsonerr(d, "bad literal");
		w[n] = getc(d);
	}
	w[n] = 0;
	t->s = nil;
	t->n = 0;
	if(strcmp(w, "true") == 0 || strcmp(w, "false") == 0){
		t->type = JSONbool;
		t->b = w[0] == 't';
	}else if(strcmp(w, "null") == 0)
		t->type = JSONnull;
	else if(n == 0)
		return _jsonerr(d, "unexpected character %#x", c);
	else
		return _jsonerr(d, "bad literal %s", w);
	return 0;
}

static int
push(Jsondec *d, int c)
{
	uchar *s;

	if(d->nstk == Jsondepth)
		return _jsonerr(d, "too deep");
	if(d->nstk == d->astk){
		d->astk = d->astk ? 2*d->astk : 16;
		if((s = realloc(d->stk, d->astk)) == nil)
			return _jsonerr(d, "out of memory");
		d->stk = s;
	}
	d->stk[d->nstk++] = c;
	return 0;
}

/* afterval sets the state for after a value. */
static void
afterval(Jsondec *d)
{
	if(d->nstk == 0)
		d->state = Ttop;
	else if(d->stk[d->nstk-1] == '[')
		d->state = Tarrval;
	else
		d->state = Tobjval;
}

static int
valueok(Jsondec *d)
{
	switch(d->state){
	case Ttop:
	case Tarrstart:
	case Tarrcomma:
	case Tobjcolon:
		return 1;
	}
	return 0;
}

/*
 * jsontoken gives the next token: 1, or 0 at the end of the input (between values), or -1.
 * An object's keys are JSONstring tokens.
 */
int
jsontoken(Jsondec *d, Jsontok *t)
{
	int c;

	if(d->err[0])
		return _jsonerr(d, "");
	memset(t, 0, sizeof *t);
	for(;;){
		c = peeknonspace(d);
		switch(c){
		case -1:
			if(d->state == Ttop)
				return 0;
			return _jsonerr(d, "unexpected end");
		case ':':
			if(d->state != Tobjkey)
				return _jsonerr(d, "unexpected ':'");
			getc(d);
			d->state = Tobjcolon;
			continue;
		case ',':
			if(d->state == Tarrval)
				d->state = Tarrcomma;
			else if(d->state == Tobjval)
				d->state = Tobjcomma;
			else
				return _jsonerr(d, "unexpected ','");
			getc(d);
			continue;
		case '[':
		case '{':
			if(!valueok(d))
				return _jsonerr(d, "unexpected '%c'", c);
			if(push(d, c) < 0)
				return -1;
			getc(d);
			d->state = c == '[' ? Tarrstart : Tobjstart;
			t->type = c;
			return 1;
		case ']':
			if(d->state != Tarrstart && d->state != Tarrval)
				return _jsonerr(d, "unexpected ']'");
			goto End;
		case '}':
			if(d->state != Tobjstart && d->state != Tobjval)
				return _jsonerr(d, "unexpected '}'");
		End:
			getc(d);
			d->nstk--;
			afterval(d);
			t->type = c;
			return 1;
		case '"':
			if(d->state == Tobjstart || d->state == Tobjcomma){
				if(str(d, t) < 0)
					return -1;
				d->state = Tobjkey;
				return 1;
			}
			if(!valueok(d))
				return _jsonerr(d, "unexpected string");
			if(str(d, t) < 0)
				return -1;
			afterval(d);
			return 1;
		}
		if(!valueok(d))
			return _jsonerr(d, "unexpected '%c'", c);
		if(c == '-' || isdigit(c)){
			if(num(d, t) < 0)
				return -1;
		}else if(lit(d, t) < 0)
			return -1;
		/* a number or a literal ends where something else begins: 01 is no 0 then 1 */
		c = peekc(d);
		if(c >= 0 && strchr(" \t\n\r,]}", c) == nil)
			return _jsonerr(d, "unexpected '%c' after a value", c);
		afterval(d);
		return 1;
	}
}

/* jsonmore is whether the array or object being read has another element. */
int
jsonmore(Jsondec *d)
{
	int c;

	if(d->err[0])
		return 0;
	c = peeknonspace(d);
	return c >= 0 && c != ']' && c != '}';
}

/* prepvalue takes what comes before a value (an array's ',', a key's ':'), as Go's tokenPrepareForDecode. */
static int
prepvalue(Jsondec *d)
{
	int c;

	if(d->err[0])
		return _jsonerr(d, "");
	c = peeknonspace(d);
	if(d->state == Tarrval){
		if(c != ',')
			return _jsonerr(d, "want ',' in an array");
		getc(d);
		d->state = Tarrcomma;
	}else if(d->state == Tobjkey){
		if(c != ':')
			return _jsonerr(d, "want ':' after a key");
		getc(d);
		d->state = Tobjcolon;
	}
	if(!valueok(d))
		return _jsonerr(d, "want a value");
	c = peeknonspace(d);
	if(c < 0)
		return _jsonerr(d, "unexpected end");
	return 0;
}

/* skip reads one value, all of it. */
static int
skip(Jsondec *d)
{
	Jsontok t;
	int depth, r;

	depth = 0;
	do{
		if((r = jsontoken(d, &t)) <= 0)
			return r < 0 ? -1 : _jsonerr(d, "unexpected end");
		if(t.type == '[' || t.type == '{')
			depth++;
		else if(t.type == ']' || t.type == '}')
			depth--;
	}while(depth > 0);
	return 0;
}

/* jsonskip reads the next value and drops it. */
int
jsonskip(Jsondec *d)
{
	if(prepvalue(d) < 0)
		return -1;
	return skip(d);
}

/* jsonraw reads the next value and gives its text as it came, as Go's json.RawMessage. */
int
jsonraw(Jsondec *d, Jsonraw *r)
{
	int n;

	if(prepvalue(d) < 0)
		return -1;
	d->nrec = 0;
	d->recording = 1;
	n = skip(d);
	d->recording = 0;
	if(n < 0)
		return -1;
	r->s = malloc(d->nrec+1);
	if(r->s == nil)
		return _jsonerr(d, "out of memory");
	memmove(r->s, d->rec, d->nrec);
	r->s[d->nrec] = 0;
	r->n = d->nrec;
	return 0;
}
