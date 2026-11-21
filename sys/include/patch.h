#pragma	src	"/sys/src/libpatch"
#pragma	lib	"libpatch.a"

typedef struct Line	Line;
typedef struct Text	Text;
typedef struct Hunk	Hunk;
typedef struct Diff	Diff;
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

struct Line
{
	char	*s;
	int	n;
};

struct Text
{
	Line	*l;
	int	n;
};

struct Hunk
{
	int	oldln;
	int	oldcnt;
	int	newln;
	int	newcnt;
	int	lead;
	int	trail;
	Text	old;
	Text	new;
	int	lnum;
};

struct Diff
{
	char	*old;
	char	*new;
	int	oldmode;
	int	newmode;
	int	flags;
	Hunk	*h;
	int	nh;
	int	lnum;
};

struct Patch
{
	Diff	*d;
	int	nd;
	char	*buf;
};

Patch*	parsepatch(char*, long, int);
void	freepatch(Patch*);
void	revpatch(Patch*);
void	splittext(Text*, char*, long);
int	findhunk(Diff*, Hunk*, Text*, int, int);
int	placehunks(Diff*, Text*, int*);
char*	applydiff(Diff*, Text*, long*);

extern void*	emalloc(ulong);
extern void*	erealloc(void*, ulong);
