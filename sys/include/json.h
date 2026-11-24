#pragma	src	"/sys/src/libjson"
#pragma	lib	"libjson.a"

typedef struct Jsondec	Jsondec;
typedef struct Jsontok	Jsontok;
typedef struct Jsonfield	Jsonfield;
typedef struct Jsonraw	Jsonraw;
typedef struct Jsonenc	Jsonenc;

enum
{
	JSONnull = 1,
	JSONbool,
	JSONnumber,
	JSONstring,

	Jstring = 1,
	Jint,
	Jfloat,
	Jnumber,
	Jbool,
	Jraw,
	Jobject,
	Jfunc,

	Jsondepth = 10000,
};

struct Jsontok
{
	int	type;
	int	b;
	char	*s;
	long	n;
};

struct Jsonraw
{
	char	*s;
	long	n;
};

struct Jsonfield
{
	char	*name;
	int	kind;
	long	off;
	Jsonfield	*sub;
	int	(*fn)(Jsondec*, void*);
};

struct Jsonenc
{
	Fmt	*f;
	int	depth;
	uchar	first[Jsondepth/8+1];
	uchar	isobj[Jsondepth/8+1];
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
