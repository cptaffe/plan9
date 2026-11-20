#pragma	src	"/sys/src/libtok"
#pragma	lib	"libtok.a"

typedef struct Tok	Tok;
typedef struct Toks	Toks;
typedef struct Tpat	Tpat;
typedef struct Tmatch	Tmatch;
typedef struct Tnames	Tnames;

enum
{
	Tmaxbind = 16,
};

struct Tnames
{
	char	**s;
	int	n;
};

struct Tok
{
	long	q0;
	long	q1;
	char	*s;
	int	id;
	Tnames	*name;
	Tnames	*tag;
	int	link;
	int	comment;
};

struct Toks
{
	Tok	*t;
	int	n;
	long	nr;
	void	*aux;
};

struct Tmatch
{
	int	t0;
	int	t1;
	int	bind[Tmaxbind];
};

Toks*	tokparse(char *text, long ntext, char *lines, long nlines);
void	tokfree(Toks *t);
int	tokat(Toks *t, long q);
int	tokbefore(Toks *t, long q);
int	tokhas(Tnames *n, char *name);

Tpat*	tokcomp(char *pat);
void	tokpatfree(Tpat *p);
int	toknbind(Tpat *p);
char*	tokbindname(Tpat *p, int i);
int	tokexec(Tpat *p, Toks *t, int t0, int t1, int from, Tmatch *m);
int	tokbexec(Tpat *p, Toks *t, int t0, int t1, Tmatch *m);

extern void*	emalloc(ulong);
extern void*	erealloc(void*, ulong);
