typedef struct Mdp	Mdp;
typedef struct Mdn	Mdn;
typedef struct Mdbuf	Mdbuf;
typedef struct Mdseg	Mdseg;
typedef struct Mdlab	Mdlab;
typedef struct Mdsum	Mdsum;
typedef struct Mdkid	Mdkid;
typedef struct Mdentity	Mdentity;
typedef struct Akid	Akid;
typedef struct Seq	Seq;
typedef struct Tn	Tn;
typedef struct Seg	Seg;
typedef struct Mk	Mk;
typedef struct Blk	Blk;
typedef struct Arena	Arena;
typedef struct P	P;
typedef struct Sub	Sub;
typedef struct Dl	Dl;
typedef struct Br	Br;

enum
{
	Pbuf = 1,
	Pcont,
	Prun,

	Chunk = 1024,
	Kmin = 6,
	Kmax = 12,
	Maxlabel = 1000,

	Fnew = 1<<20,
	Fblank = 1<<21,
	Fends = 1<<22,
	Fla = 1<<23,
	Flb = 1<<24,
	Fquote = 1<<25,

	Shb = 1<<0,
	Sea = 1<<1,
	Sel = 1<<2,
	Shi = 1<<3,
	Sia = 1<<4,
	Sila = 1<<5,
	Silb = 1<<6,

	Rroot = 0,
	Rseq,
	Rrun,
	Rbuf,
	Rnode,
};

struct Mdp
{
	int	ref;
	int	kind;
};

struct Mdn
{
	uchar	type;
	uchar	c;
	ushort	w;
	u32int	flags;
	u32int	aux;
	u32int	off;
	u32int	len;
	u32int	line;
	u32int	kid;
	u32int	next;
};

struct Mdseg
{
	u32int	node;
	u32int	b;
	u32int	e;
	u32int	ln;
	u32int	line;
	uchar	pad;
	uchar	nl;
};

struct Mdlab
{
	u32int	hash;
	u32int	node;
};

struct Mdsum
{
	uchar	f;
	uvlong	bloom;
};

struct Mdbuf
{
	Mdp;
	Mdn	*n;
	int	nn;
	Mdseg	*seg;
	int	nseg;
	Mdlab	*lab;
	int	nlab;
};

struct Mdkid
{
	Mdp	*p;
	u32int	d;
	u32int	dl;
	u32int	len;
	Mdsum	s;
};

struct Mdtree
{
	Mdp;
	int	h;
	Mdkid	*k;
	int	nk;
	Mdn	n;
};

struct Mdframe
{
	int	kind;
	Mdp	*p;
	int	i;
	u32int	q;
	u32int	l;
};

struct Mdentity
{
	char	*name;
	Rune	r[2];
};

struct Akid
{
	Mdp	*p;
	u32int	q;
	u32int	l;
	u32int	len;
	Mdsum	s;
};

struct Seq
{
	Akid	*k;
	int	nk;
	int	h;
};

struct Seg
{
	u32int	b;
	u32int	e;
	u32int	ln;
	u32int	line;
	uchar	pad;
	uchar	nl;
};

struct Tn
{
	uchar	type;
	uchar	c;
	ushort	w;
	u32int	flags;
	u32int	aux;
	u32int	q0;
	u32int	q1;
	u32int	line;
	Tn	*kid;
	Tn	*last;
	Tn	*next;
	Tn	*prev;
	Tn	*up;
	Seg	*seg;
	int	nseg;
	u32int	*lab;
	int	nlab;
	char	*key;
	Tn	*scan;
	Akid	a;
};

struct Mk
{
	u32int	q0;
	u32int	q1;
	u32int	line;
	uchar	aux;
	Blk	*own;
};

struct Blk
{
	uchar	type;
	uchar	c;
	ushort	w;
	u32int	flags;
	u32int	aux;
	u32int	q0;
	u32int	q1;
	u32int	line;
	int	open;
	int	llb;
	int	lllb;
	int	tv;
	int	moff;
	int	pad;
	int	foff;
	Blk	*up;
	Blk	*kid;
	Blk	*last;
	Blk	*next;
	Blk	*prev;
	Seq	old;
	int	isold;
	Tn	*oldk;
	Seg	*seg;
	int	nseg;
	int	mseg;
	Tn	*tn;
	Mk	*mk;
	int	nmk;
	int	mmk;
	int	ncol;
	uchar	*align;
	int	zip;
	u32int	zflags;
	u32int	zq1;
};

struct Arena
{
	Arena	*next;
	uchar	*p;
	uchar	*e;
};

struct P
{
	char	*s;
	ulong	n;
	int	ext;
	uchar	*b;
	int	nb;
	int	mb;
	ulong	ls;
	ulong	le;
	ulong	ln;
	ulong	line;
	int	off;
	int	col;
	int	fns;
	int	fnscol;
	int	indent;
	int	blank;
	int	ptab;
	int	tbkill;
	Blk	*root;
	Blk	*cur;
	Mk	*mk;
	int	nmk;
	int	mmk;
	Blk	*opened;
	Arena	*a;
	char	**key;
	int	nkey;
	int	mkey;
	Mdref	*def;
	int	ndef;
	int	mdef;
};

struct Sub
{
	P	*p;
	uchar	*s;
	int	n;
	int	pos;
	Seg	*seg;
	int	nseg;
	u32int	*vs;
	Tn	*root;
	Dl	*delim;
	Br	*brack;
	int	nolink;
	int	flags;
	int	bt[1001];
	int	btscan;
	u32int	*lab;
	int	nlab;
	int	mlab;
};

extern Rune	_mdpunct[][2];
extern int	_mdnpunct;
extern Rune	_mdfold[][4];
extern int	_mdnfold;
extern Mdentity	_mdent[];
extern int	_mdnent;

void*	_mdalloc(P*, ulong);
void	_mdfreearena(P*);
Tn*	_mdtn(P*, int, u32int, u32int, u32int);
void	_mdadd(Tn*, Tn*);
void	_mdunlink(Tn*);
void	_mdbefore(Tn*, Tn*);

int	_mdpunctr(Rune);
int	_mdspacer(Rune);
int	_mdispunct(int);
int	_mdisspace(int);
int	_mdutf(uchar*, int, int, Rune*);
int	_mdfoldr(Rune, Rune*);
int	_mdent1(uchar*, int, Rune*);

int	_mdscanhtag(uchar*, int, int);
int	_mdscanauri(uchar*, int, int);
int	_mdscanamail(uchar*, int, int);
int	_mdscantitle(uchar*, int, int);
int	_mdscanspace(uchar*, int, int);
int	_mdscancomment(uchar*, int, int);
int	_mdscancdata(uchar*, int, int);
int	_mdscanatx(uchar*, int, int);
int	_mdscansetext(uchar*, int, int);
int	_mdscanopenfence(uchar*, int, int);
int	_mdscanclosefence(uchar*, int, int);
int	_mdscanhstart(uchar*, int, int);
int	_mdscanhstart7(uchar*, int, int);
int	_mdscanhend(uchar*, int, int, int);
int	_mdscantstart(uchar*, int, int);
int	_mdscantcell(uchar*, int, int);
int	_mdscantcellend(uchar*, int, int);
int	_mdscantrowend(uchar*, int, int);
int	_mdscantask(uchar*, int, int);

char*	_mdnorm(uchar*, int);
u32int	_mdhash(char*);
int	_mdlookup(P*, char*);
void	_mdaddkey(P*, char*);
void	_mdinline(P*, Tn*, Seg*, int, int);
int	_mdparseref(P*, Seg*, int, Blk*, Tn**);
void	_mdtnmap(Sub*, Tn*);

P*	_mdnewp(char*, ulong);
void	_mdline(P*, ulong);
void	_mdfinish(P*);
Blk*	_mdfinal(P*, Blk*);
void	_mdphase2(P*, Blk*);

void	_mdincref(Mdp*);
void	_mddecref(Mdp*);
Mdsum	_mdsumcat(Mdsum, Mdsum);
Mdsum	_mdsumtn(Tn*);
Mdbuf*	_mdencode(Tn*, Tn*, u32int, u32int);
Tn*	_mddecode(P*, Mdbuf*, u32int, u32int);
Tn*	_mddecodekid(P*, Akid*);
Mdtree*	_mdmkcont(Tn*, Seq);
Seq	_mdseqcat(Seq, Seq);
Seq	_mdseqof(Akid*, int);
void	_mdseqbuild(Seq*);
Seq	_mdseqsplit(Seq, u32int, Seq*);
void	_mdseqfree(Seq);
Akid	_mdakid(Mdp*, u32int, u32int);
void	_mdkids(Mdtree*, u32int, u32int, Akid**, int*);
Tn*	_mdpack(P*, Blk*);
Mdtree*	_mdpackroot(P*, Blk*);
void	_mdcontflags(Tn*, Mdsum);
void	_mdinsmark(Tn*, Tn*);
int	_mdisblock(int);

void	_mdcurpush(Mdcur*, int, Mdp*, int, u32int, u32int);
Mdn*	_mdcurn(Mdcur*);
void	_mdcurfill(Mdcur*);
