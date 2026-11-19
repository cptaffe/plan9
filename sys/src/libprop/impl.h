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
	char	*tag;
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
	char	*err;

	int	child;
	int	ofd;
	uchar	*ob;
	int	nob;
	int	mob;
	int	oall;
};

Src*	_propnew(int);
void	_propreset(Src*);
uvlong	_propnext(Src*, uvlong, uvlong);
void	_proprec(Src*, uvlong, uvlong);
void	_propopen(Src*, char*, int);
void	_propnote(Src*, char*);
uvlong	_proprnd(Src*);
char*	_propintern(char*, int);
int	_proprun(Src*, Prop*, void*, char**);
int	_propfork(Src*, Prop*, void*, char**, int);
void	_propnotify(void);
void	_propput(Src*, int, uchar*, int);
void	_propflush(Src*);
char*	_propnotes(Src*);
char*	_propdup(char*);

extern int	_propstop;
