#pragma	src	"/sys/src/libpatch"
#pragma	lib	"libpatch.a"

typedef struct Pline	Pline;
typedef struct Ptext	Ptext;
typedef struct Phunk	Phunk;
typedef struct Pdiff	Pdiff;
typedef struct Patch	Patch;

enum
{
	Pgit	= 1<<0,
	Pnew	= 1<<1,
	Pdel	= 1<<2,
	Pren	= 1<<3,
	Pcopy	= 1<<4,
	Pzero	= 1<<5,
};

struct Pline
{
	char	*s;
	int	n;
};

struct Ptext
{
	Pline	*l;
	int	n;
};

struct Phunk
{
	int	oldln;
	int	oldcnt;
	int	newln;
	int	newcnt;
	int	lead;
	int	trail;
	Ptext	old;
	Ptext	new;
	int	lnum;
};

struct Pdiff
{
	char	*old;
	char	*new;
	int	oldmode;
	int	newmode;
	int	flags;
	Phunk	*h;
	int	nh;
	int	lnum;
};

struct Patch
{
	Pdiff	*d;
	int	nd;
	char	*buf;
};

Patch*	parsepatch(char*, long, int);
void	freepatch(Patch*);
void	revpatch(Patch*);
void	splittext(Ptext*, char*, long);
int	findhunk(Pdiff*, Phunk*, Ptext*, int, int);
int	placehunks(Pdiff*, Ptext*, int*);
char*	applydiff(Pdiff*, Ptext*, long*);

extern void*	emalloc(ulong);
extern void*	erealloc(void*, ulong);
