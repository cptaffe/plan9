typedef struct Pen	Pen;
typedef struct Saved	Saved;
typedef struct Priv	Priv;

enum
{
	Nparam = 32,
	Nosc = 4096,
	Ndumb = 256,

	/* the parser's states */
	Ground = 0,
	Esc,
	Escint,
	Csi,
	Csiignore,
	Osc,
	Oscesc,
	Str,	/* DCS, SOS, PM and APC, to their ST */
	Stresc,
};

struct Pen
{
	ulong	fg;
	ulong	bg;
	ushort	attr;
	ushort	link;
};

/* what DECSC keeps */
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
	int	top;	/* the scroll region, rows top to bot-1 */
	int	bot;
	Pen	pen;
	int	pending;	/* the last column was written: the next rune wraps */
	uchar	*tabs;
	int	g[2];	/* G0 and G1: 1 for DEC's line drawing */
	int	shift;	/* SO: G1 is used */
	Saved	saved[2];	/* the main screen's and the alternate's */
	Rune	last;	/* for REP */
	void	(*dumb)(Vt*, Rune*, int);	/* vtdumb's: the text goes here, and no screen is kept */
	Rune	dbuf[Ndumb];
	int	ndbuf;

	int	state;
	int	param[Nparam];
	uchar	sub[Nparam];	/* param[i] came after a colon */
	int	nparam;
	int	priv;	/* CSI's ? > = or <, or 0 */
	int	inter;	/* the intermediate byte, or 0 */
	char	osc[Nosc];
	int	nosc;
	uchar	utf[UTFmax];
	int	nutf;

	/* this frame's damage */
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
	int	cleardirty;	/* the last frame's dirty rows are still marked */
};

#define P(vt)	((Priv*)(vt)->priv)

void	_vtbegin(Vt*);
void	_vtblank(Vt*, Vtcell*, int);
void	_vttouch(Vt*, int);
void	_vttouchall(Vt*);
void	_vtscroll(Vt*, int, int, int, int);
void	_vtsoftreset(Vt*);
char*	_vtstrdup(char*);
