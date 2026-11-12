#pragma	src	"/sys/src/libjson"
#pragma	lib	"libjson.a"

/*
 * JSON, as Go's encoding/json has it: a decoder that gives a stream's tokens, decodes an
 * object into a struct by a table of its fields, and keeps a value's text as it came (a raw
 * value); an encoder that writes values to a Fmt. Numbers are their text. See json(2).
 * It needs <bio.h> first.
 */

typedef struct Jsondec	Jsondec;
typedef struct Jsontok	Jsontok;
typedef struct Jsonfield	Jsonfield;
typedef struct Jsonraw	Jsonraw;
typedef struct Jsonenc	Jsonenc;

enum
{
	/* a token's type; '{', '}', '[' and ']' are their own */
	JSONnull = 1,
	JSONbool,
	JSONnumber,
	JSONstring,

	/* a field's kind */
	Jstring = 1,	/* char*: the decoded string, malloc'd */
	Jint,	/* vlong */
	Jfloat,	/* double */
	Jnumber,	/* char*: the number's text, malloc'd */
	Jbool,	/* int */
	Jraw,	/* Jsonraw: the value's text, whatever it is */
	Jobject,	/* a struct, by the table sub */
	Jfunc,	/* fn decodes the value itself */

	Jsondepth = 10000,	/* containers within containers, at most */
};

struct Jsontok
{
	int	type;
	int	b;	/* JSONbool's value */
	char	*s;	/* JSONstring's decoded text, or JSONnumber's text: until the next call */
	long	n;	/* its length (a string may hold \u0000) */
};

struct Jsonraw
{
	char	*s;	/* malloc'd, and NUL-terminated */
	long	n;
};

struct Jsonfield
{
	char	*name;
	int	kind;
	long	off;	/* of the member, from the struct's start */
	Jsonfield	*sub;	/* Jobject's fields, ending in one with a nil name */
	int	(*fn)(Jsondec*, void*);	/* Jfunc's */
};

struct Jsonenc
{
	Fmt	*f;
	int	depth;
	uchar	first[Jsondepth/8+1];	/* bit d: no value yet at depth d */
	uchar	isobj[Jsondepth/8+1];	/* bit d: the container at depth d is an object */
	int	afterkey;
	int	err;
};

Jsondec*	jsondec(Biobuf*);
Jsondec*	jsondecmem(char*, long);
void	jsondecfree(Jsondec*);
int	jsontoken(Jsondec*, Jsontok*);
int	jsonmore(Jsondec*);
int	jsonskip(Jsondec*);
int	jsonraw(Jsondec*, Jsonraw*);
int	jsondecode(Jsondec*, Jsonfield*, void*);

void	jsonenc(Jsonenc*, Fmt*);
int	jsonbegin(Jsonenc*, int);
int	jsonend(Jsonenc*);
int	jsonkey(Jsonenc*, char*);
int	jsonstring(Jsonenc*, char*);
int	jsonstringn(Jsonenc*, char*, long);
int	jsonnumber(Jsonenc*, char*);
int	jsonint(Jsonenc*, vlong);
int	jsonbool(Jsonenc*, int);
int	jsonnull(Jsonenc*);
int	jsonrawval(Jsonenc*, char*, long);
int	jsonquote(Fmt*, char*, long);
