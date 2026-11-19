#pragma	src	"/sys/src/libmd"
#pragma	lib	"libmd.a"

typedef struct Md	Md;
typedef struct Mdnode	Mdnode;
typedef struct Mdcur	Mdcur;
typedef struct Mdframe	Mdframe;
typedef struct Mdtree	Mdtree;
typedef struct Mdref	Mdref;

enum
{
	Mddoc = 1,
	Mdquote,
	Mdlist,
	Mditem,
	Mdpara,
	Mdhead,
	Mdcode,
	Mdhtml,
	Mdrule,
	Mddef,
	Mdtable,
	Mdrow,
	Mdcell,

	Mdtext,
	Mdesc,
	Mdent,
	Mdsoft,
	Mdhard,
	Mdcodespan,
	Mdemph,
	Mdstrong,
	Mdstrike,
	Mdlink,
	Mdimage,
	Mdauto,
	Mdraw,

	Mdmark,
	Mddest,
	Mdtitle,
	Mdlabel,
	Mdinfo,
	Mdtask,
	Mdntype,
};

enum
{
	Mdxtable = 1<<0,
	Mdxstrike = 1<<1,
	Mdxauto = 1<<2,
	Mdxtask = 1<<3,
	Mdgfm = Mdxtable|Mdxstrike|Mdxauto|Mdxtask,
};

enum
{
	Mdordered = 1<<0,
	Mdtight = 1<<1,
	Mdfenced = 1<<2,
	Mdsetext = 1<<3,
	Mdheader = 1<<4,
	Mdtodo = 1<<5,
	Mdchecked = 1<<6,
	Mdinline = 1<<7,
	Mdfull = 1<<8,
	Mdcollapsed = 1<<9,
	Mdshortcut = 1<<10,
	Mdemail = 1<<11,
	Mdwww = 1<<12,
	Mdbare = 1<<13,
	Mdfill = 1<<14,
};

struct Mdnode
{
	int	type;
	int	flags;
	int	aux;
	int	c;
	int	w;
	ulong	q0;
	ulong	q1;
	ulong	line;
};

struct Mdref
{
	char	*key;
	u32int	hash;
	u32int	off;
};

struct Md
{
	char	*s;
	ulong	n;
	ulong	seq;
	int	ext;
	ulong	r0;
	ulong	r1;
	Mdtree	*root;
	Mdref	*ref;
	int	nref;
};

struct Mdcur
{
	Mdnode;
	Md	*md;
	Mdframe	*f;
	int	nf;
	int	mf;
};

Md*	mdparse(char*, ulong, int);
Md*	mdedit(Md*, char*, ulong, ulong, ulong, ulong);
void	mdfree(Md*);

void	mdroot(Md*, Mdcur*);
int	mddown(Mdcur*);
int	mdnext(Mdcur*);
int	mdup(Mdcur*);
int	mdseek(Mdcur*, ulong);
void	mdcurcopy(Mdcur*, Mdcur*);
void	mdcurfree(Mdcur*);

int	mddef(Mdcur*, Mdcur*);
int	mdentity(char*, int, Rune*);

extern void*	emalloc(ulong);
extern void*	erealloc(void*, ulong);
