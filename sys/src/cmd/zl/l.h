#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"../zc/z.out.h"

typedef	struct	Adr	Adr;
typedef	struct	Prog	Prog;
typedef	struct	Sym	Sym;
typedef	struct	Sig	Sig;
typedef	struct	Buf	Buf;
typedef	struct	Rel	Rel;

#define	P	((Prog*)0)
#define	S	((Sym*)0)

enum
{
	NHASH	= 10007,
	NARG	= 64,
	STACK	= 1<<20,
	PAGE	= 1<<16,

	SXREF	= 0,
	STEXT,
	SDATA,
	SBSS,
	SIMPORT,
	SWASM,
	SLINK,
	SUNDEF,

	LABEL	= 1<<0,
	ARGV	= 1<<1,
};

enum
{
	WUNREACHABLE	= 0x00,
	WBLOCK	= 0x02,
	WLOOP	= 0x03,
	WIF	= 0x04,
	WTRY	= 0x06,
	WCATCH	= 0x07,
	WTHROW	= 0x08,
	WRETHROW	= 0x09,
	WELSE	= 0x05,
	WEND	= 0x0b,
	WBR	= 0x0c,
	WBRIF	= 0x0d,
	WBRTABLE	= 0x0e,
	WRETURN	= 0x0f,
	WCALL	= 0x10,
	WCALLINDIRECT	= 0x11,
	WDROP	= 0x1a,
	WLOCALGET	= 0x20,
	WLOCALSET	= 0x21,
	WLOCALTEE	= 0x22,
	WGLOBALGET	= 0x23,
	WGLOBALSET	= 0x24,
	WI32LOAD	= 0x28,
	WI64LOAD	= 0x29,
	WF32LOAD	= 0x2a,
	WF64LOAD	= 0x2b,
	WI32LOAD8S	= 0x2c,
	WI32LOAD8U	= 0x2d,
	WI32LOAD16S	= 0x2e,
	WI32LOAD16U	= 0x2f,
	WI32STORE	= 0x36,
	WI64STORE	= 0x37,
	WF32STORE	= 0x38,
	WF64STORE	= 0x39,
	WI32STORE8	= 0x3a,
	WI32STORE16	= 0x3b,
	WMEMSIZE	= 0x3f,
	WMEMGROW	= 0x40,
	WI32CONST	= 0x41,
	WI64CONST	= 0x42,
	WF64CONST	= 0x44,
	WI32EQ	= 0x46,
	WI32NE	= 0x47,
	WI32ADD	= 0x6a,
	WI32SUB	= 0x6b,
	WI32AND	= 0x71,
	WF64NEG	= 0x9a,
	WI32WRAP	= 0xa7,
	WI64EXTENDU	= 0xad,
	WF32DEMOTE	= 0xb6,
	WF64PROMOTE	= 0xbb,
	WI32EXTEND8	= 0xc0,
	WI32EXTEND16	= 0xc1,
	WPREFIX	= 0xfc,
	WPREFIXV	= 0xfd,
	WPREFIXA	= 0xfe,
	WVOID	= 0x40,
};

struct	Adr
{
	vlong	offset;
	char	sval[NSNAME];
	Sym*	sym;
	char	type;
	char	reg;
	char	name;
};

struct	Prog
{
	Adr	from;
	Adr	to;
	Prog*	link;
	Prog*	cond;
	Prog**	tab;
	int	ntab;
	long	line;
	int	lab;
	int	sig;
	int	blk;
	uchar	as;
	char	reg;
	uchar	mark;
};

struct	Sig
{
	int	np;
	char	p[NARG];
	char	r;
};

struct	Sym
{
	char*	name;
	int	version;
	short	type;
	char	reach;
	char	ref;
	char	dupok;
	long	value;
	long	size;
	int	fidx;
	int	tidx;
	int	sig;
	int	ridx;
	int	dver;
	long	frame;
	long	pframe;
	char*	file;
	Prog*	text;
	Prog*	decl;
	Prog*	data;
	Prog*	edata;
	Sym*	hash;
	Sym*	next;
};

struct	Rel
{
	int	type;
	long	off;
	Sym*	sym;
	Sym*	in;
	long	idx;
	long	add;
};

enum
{
	RFUNC	= 0,
	RTABSLEB	= 1,
	RTABI32	= 2,
	RMEMSLEB	= 4,
	RMEMI32	= 5,
	RTYPE	= 6,
	RGLOBAL	= 7,
	RTAG	= 10,
};

struct	Buf
{
	uchar*	p;
	long	n;
	long	cap;
};

extern	Sym*	hash[NHASH];
extern	Sym*	textp;
extern	Sym*	etextp;
extern	Sym*	datap;
extern	Sig*	sigs;
extern	int	nsig;
extern	int	nerrors;
extern	char*	outfile;
extern	long	stacksize;
extern	char*	entry;
extern	char**	exports;
extern	int	nexport;
extern	int	rflag;
extern	int	simd;
extern	Rel*	rels;
extern	int	nrel;
extern	Sym*	cursym;
extern	int	debug[128];
extern	char*	anames[];

Sym*	lookup(char*, int);
void	diag(char*, ...);
void	errorexit(void);
void	ldobj(char*);
void	loadlib(void);
void	reach(void);
void	layout(void);
void	asmb(void);
int	sigof(Sig*);
void	funcsig(Sym*, Sig*);
int	isimport(Sym*);
int	iswasm(Sym*);
int	isexport(Sym*);
int	issetjmp(Prog*);
char*	impmod(Sym*, char**);
void	gencode(Sym*, Buf*);
long	symaddr(Sym*);
int	isfunc(Sym*);
void	addrel(int, long, Sym*, Sym*, long, long);
void	bpad(Buf*, vlong);
void	asmr(void);

void	bput(Buf*, int);
void	bputs(Buf*, void*, long);
void	buleb(Buf*, uvlong);
void	bsleb(Buf*, vlong);
void	bname(Buf*, char*);
void	bvec(Buf*, Buf*);
void	bfree(Buf*);

#pragma	varargck	argpos	diag	1
