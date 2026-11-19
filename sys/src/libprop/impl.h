typedef struct Span	Span;

enum
{
	Rand,
	Replay,
	Bytes,

	Pass = 0,
	Fail,
	Discard,

	Maxchoice = 1<<14,
};

struct Span
{
	int	a;
	int	e;
	int	d;
	void	*tag;
};

struct Src
{
	int	mode;
	uvlong	x;
	int	size;
	uvlong	*in;
	int	nin;
	int	i;
	uchar	*b;
	long	nb;
	long	ib;

	uvlong	*v;
	uvlong	*n;
	int	nv;
	int	mv;
	Span	*sp;
	int	nsp;
	int	msp;
	int	*open;
	int	nopen;
	int	mopen;

	int	jok;
	jmp_buf	j;
	Fmt	notes;
	int	nnotes;
};

Src*	_srcnew(int);
void	_srcreset(Src*);
uvlong	_srcnext(Src*, uvlong, uvlong);
uvlong	_srcrnd(Src*);
void	_srctag(Src*, void*);
int	_srcrun(Src*, Prop*, void*, char**);
char*	_srcnotes(Src*);
void*	_propalloc(ulong);
void*	_proprealloc(void*, ulong);
