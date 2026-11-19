#include	"../cc/cc.h"
#include	"../zc/z.out.h"

#define	SZ_CHAR		1
#define	SZ_SHORT	2
#define	SZ_INT		4
#define	SZ_LONG		4
#define	SZ_IND		4
#define	SZ_FLOAT	4
#define	SZ_VLONG	8
#define	SZ_DOUBLE	8
#define	FNX		100

typedef	struct	Adr	Adr;
typedef	struct	Prog	Prog;
typedef	struct	Case	Case;
typedef	struct	C1	C1;

struct	Adr
{
	vlong	offset;
	double	dval;
	char	sval[NSNAME];

	Sym*	sym;
	char	type;
	char	reg;
	char	name;
	char	etype;
	long	width;
};
#define	A	((Adr*)0)

#define	INDEXED	9
struct	Prog
{
	Adr	from;
	Adr	to;
	Prog*	link;
	long	lineno;
	char	as;
	char	reg;
};
#define	P	((Prog*)0)

struct	Case
{
	Case*	link;
	vlong	val;
	long	label;
	char	def;
	char	isv;
};
#define	C	((Case*)0)

struct	C1
{
	vlong	val;
	long	label;
};

EXTERN	long	breakpc;
EXTERN	long	nbreak;
EXTERN	Case*	cases;
EXTERN	Node	constnode;
EXTERN	Node	fconstnode;
EXTERN	Node	vconstnode;
EXTERN	long	continpc;
EXTERN	long	curarg;
EXTERN	long	cursafe;
EXTERN	Prog*	firstp;
EXTERN	Prog*	lastp;
EXTERN	long	maxargsafe;
EXTERN	int	mnstring;
EXTERN	Node*	nodrat;
EXTERN	Node*	nodret;
EXTERN	Node*	nodsafe;
EXTERN	long	nrathole;
EXTERN	long	nstring;
EXTERN	Prog*	p;
EXTERN	long	pc;
EXTERN	Node	regnode;
EXTERN	Node	vregnode;
EXTERN	Node	fregnode;
EXTERN	char	string[NSNAME];
EXTERN	Sym*	symrathole;
EXTERN	Node	znode;
EXTERN	Prog	zprog;
EXTERN	int	reg[NALLREG];
EXTERN	int	suppress;

extern	char*	anames[];

void	codgen(Node*, Node*);
void	gen(Node*);
void	noretval(int);
void	xcom(Node*);
int	bcomplex(Node*, Node*);
void	usedset(Node*, int);

void	cgen(Node*, Node*);
void	reglcgen(Node*, Node*, Node*);
void	lcgen(Node*, Node*);
void	bcgen(Node*, int);
void	boolgen(Node*, int, Node*);
void	sugen(Node*, Node*, long);
void	gcall(Node*, Node*, Node*);

void	ginit(void);
void	gclean(void);
void	nextpc(void);
Node*	nodconst(long);
Node*	nod32const(vlong);
Node*	nodfconst(double);
Node*	nodgconst(vlong, Type*);
void	nodreg(Node*, Node*, int);
void	regret(Node*, Node*);
void	regalloc(Node*, Node*, Node*);
void	regfree(Node*);
void	regialloc(Node*, Node*, Node*);
void	regsalloc(Node*, Node*);
void	regind(Node*, Node*);
void	raddr(Node*, Prog*);
void	naddr(Node*, Adr*);
void	gmove(Node*, Node*);
void	gins(int a, Node*, Node*);
void	gopcode(int, Node*, Node*, Node*);
int	samaddr(Node*, Node*);
void	gbranch(int);
void	patch(Prog*, long);
int	sconst(Node*);
void	gpseudo(int, Sym*, Node*);
void	gdecl(Sym*);
int	rclass(int);
int	tclass(Type*);
int	kind(Type*);
int	sutype(Type*);
Type*	sumember(Type*, long*);
long	talign(Type*);
void	gprologue(Sym*);
int	extop(int);
void	gext(Node*);
void	gretval(void);

int	swcmp(const void*, const void*);
void	doswit(Node*);
void	swit1(C1*, int, long, Node*);
void	swit2(C1*, int, long, Node*, Node*);
void	casf(void);
void	bitload(Node*, Node*, Node*, Node*, Node*);
void	bitstore(Node*, Node*, Node*, Node*, Node*);
long	outstring(char*, long);
int	mulcon(Node*, Node*);
void	nullwarn(Node*, Node*);
void	gextern(Sym*, Node*, long, long);
void	outcode(void);
void	ieeedtod(Ieee*, double);

void	listinit(void);
int	Pconv(Fmt*);
int	Aconv(Fmt*);
int	Dconv(Fmt*);
int	Sconv(Fmt*);
int	Nconv(Fmt*);

void	regopt(Prog*);

#pragma	varargck	type	"A"	int
#pragma	varargck	type	"D"	Adr*
#pragma	varargck	type	"N"	Adr*
#pragma	varargck	type	"P"	Prog*
#pragma	varargck	type	"S"	char*
