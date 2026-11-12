/*
 * jsondecode: an object into a struct, by a table of its fields, as Go's json.Unmarshal does
 * into a struct: a key that no field has is skipped, and null leaves a field as it was. Keys
 * match fields' names exactly.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <json.h>
#include "impl.h"

static char*
memdup(char *s, long n)
{
	char *t;

	if((t = malloc(n+1)) == nil)
		return nil;
	memmove(t, s, n);
	t[n] = 0;
	return t;
}

static int
want(Jsondec *d, Jsonfield *f, char *what)
{
	return _jsonerr(d, "%s: want %s", f->name, what);
}

static int
field(Jsondec *d, Jsonfield *f, void *p)
{
	Jsontok t;
	char *e;
	int r;

	switch(f->kind){
	case Jraw:
		return jsonraw(d, p);
	case Jobject:
		return jsondecode(d, f->sub, p);
	case Jfunc:
		return (*f->fn)(d, p);
	}
	if((r = jsontoken(d, &t)) <= 0)
		return r < 0 ? -1 : _jsonerr(d, "unexpected end");
	if(t.type == JSONnull)
		return 0;
	switch(f->kind){
	case Jstring:
		if(t.type != JSONstring)
			return want(d, f, "a string");
		if((*(char**)p = memdup(t.s, t.n)) == nil)
			return _jsonerr(d, "out of memory");
		return 0;
	case Jint:
		if(t.type != JSONnumber || strpbrk(t.s, ".eE") != nil)
			return want(d, f, "an integer");
		*(vlong*)p = strtoll(t.s, &e, 10);
		return 0;
	case Jfloat:
		if(t.type != JSONnumber)
			return want(d, f, "a number");
		*(double*)p = strtod(t.s, &e);
		return 0;
	case Jnumber:
		if(t.type != JSONnumber)
			return want(d, f, "a number");
		if((*(char**)p = memdup(t.s, t.n)) == nil)
			return _jsonerr(d, "out of memory");
		return 0;
	case Jbool:
		if(t.type != JSONbool)
			return want(d, f, "true or false");
		*(int*)p = t.b;
		return 0;
	}
	return _jsonerr(d, "%s: bad field kind %d", f->name, f->kind);
}

/* jsondecode reads an object (or null) into v, by the fields f, which end in one with a nil name. */
int
jsondecode(Jsondec *d, Jsonfield *fs, void *v)
{
	Jsontok t;
	Jsonfield *f;
	int r;

	if((r = jsontoken(d, &t)) <= 0)
		return r < 0 ? -1 : _jsonerr(d, "unexpected end");
	if(t.type == JSONnull)
		return 0;
	if(t.type != '{')
		return _jsonerr(d, "want an object");
	while(jsonmore(d)){
		if(jsontoken(d, &t) < 0)
			return -1;
		for(f = fs; f->name != nil; f++)
			if(strlen(f->name) == t.n && memcmp(f->name, t.s, t.n) == 0)
				break;
		if(f->name == nil)
			r = jsonskip(d);
		else
			r = field(d, f, (char*)v + f->off);
		if(r < 0)
			return -1;
	}
	if(jsontoken(d, &t) < 0)
		return -1;
	if(t.type != '}')
		return _jsonerr(d, "want '}'");
	return 0;
}
