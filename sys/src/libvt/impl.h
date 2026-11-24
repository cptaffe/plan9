typedef struct Pen	Pen;
typedef struct Saved	Saved;
typedef struct Priv	Priv;

enum
{
	Nparam = 32,
	Nosc = 4096,
	Ndumb = 256,
	Nlinks = 65535,
	Nlinkbytes = 1<<20,

	Ground = 0,
	Esc,
	Escint,
	Csi,
	Csiignore,
	Osc,
	Oscesc,
	Str,
	Stresc,
};

struct Pen
{
	ulong	fg;
	ulong	bg;
	ushort	attr;
	ushort	link;
};

struct Saved
{
	int	x;
	int	y;
	Pen	pen;
	int	origin;
	int	wrap;
	int	g[2];
	int	shift;
};

struct Priv
{
	Vtline	*main;
	Vtline	*alt;
	int	top;
	int	bot;
	Pen	pen;
	int	pending;
	uchar	*tabs;
	int	g[2];
	int	shift;
	Saved	saved[2];
	Rune	last;
	void	(*dumb)(Vt*, Rune*, int);
	Rune	dbuf[Ndumb];
	int	ndbuf;

	int	state;
	int	param[Nparam];
	uchar	sub[Nparam];
	int	nparam;
	int	priv;
	int	inter;
	char	osc[Nosc];
	int	nosc;
	uchar	utf[UTFmax];
	int	nutf;

	int	all;
	uchar	*dirty;
	Vtscroll	*scroll;
	int	nscroll;
	int	ascroll;
	Vtline	ring[Vgone];
	int	head;
	int	ngone;
	long	lost;
	Vtline	out[Vgone];
	int	ox;
	int	oy;
	int	omode;
	int	title;
	int	bell;
	int	cleardirty;

	ushort	*lh;
	int	nlh;
	long	linkbytes;
};

#define P(vt)	((Priv*)(vt)->priv)

void	_vtbegin(Vt*);
void	_vtblank(Vt*, Vtcell*, int);
void	_vttouch(Vt*, int);
void	_vttouchall(Vt*);
void	_vtscroll(Vt*, int, int, int, int);
void	_vtsoftreset(Vt*);
char*	_vtstrdup(char*);
