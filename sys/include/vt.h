#pragma	src	"/sys/src/libvt"
#pragma	lib	"libvt.a"

/*
 * A terminal: xterm's escape sequences parsed into a screen of cells, with nothing drawn.
 * vtwrite takes the host's output; vtframe says what changed since the last frame; vtkey,
 * vtpaste, vtmouse and vtfocus make the bytes the host is sent, as the modes ask. A key is a
 * rune or <keyboard.h>'s (Kup, Khome, KF|n, ...), with Vctl, Valt and Vshift or'd in; Kbs
 * sends DEL, and Kdel the delete key's ESC [3~. vtwidth is a rune's columns, 0, 1 or 2, by
 * Unicode 15.1's East Asian Width and general categories, as Vwide and Vcont have them.
 * vtdumb's Vt keeps no screen: it gives its put the text, with \b \t \r and \n. It needs
 * <u.h> first. See vt(2).
 */

typedef struct Vt	Vt;
typedef struct Vtcell	Vtcell;
typedef struct Vtline	Vtline;
typedef struct Vtscroll	Vtscroll;
typedef struct Vtframe	Vtframe;

enum
{
	/* a cell's attributes */
	Vbold = 1<<0,
	Vfaint = 1<<1,
	Vitalic = 1<<2,
	Vunder = 1<<3,
	Vblink = 1<<4,
	Vreverse = 1<<5,
	Vinvisible = 1<<6,
	Vstrike = 1<<7,
	Vwide = 1<<8,	/* the first of a rune two cells wide */
	Vcont = 1<<9,	/* the second, whose rune is 0 */

	/* a color: its kind in the top byte */
	Vdefault = 0,
	Vindex = 1<<24,	/* 0 to 255 in the low byte */
	Vrgb = 2<<24,	/* 0xRRGGBB in the low three */

	/* modes */
	Mcursor = 1<<0,	/* the cursor is shown */
	Mappcursor = 1<<1,	/* DECCKM: arrows send ESC O */
	Mappkeypad = 1<<2,	/* DECKPAM */
	Mwrap = 1<<3,	/* DECAWM */
	Morigin = 1<<4,	/* DECOM */
	Minsert = 1<<5,	/* IRM */
	Mnewline = 1<<6,	/* LNM: line feed is also a return */
	Malt = 1<<7,	/* the alternate screen (47, 1047, 1049) */
	Mbracket = 1<<8,	/* 2004: bracketed paste */
	Mfocus = 1<<9,	/* 1004: focus in and out are sent */
	Mmouse = 1<<10,	/* 1000: buttons */
	Mmousedrag = 1<<11,	/* 1002: and motion while one is down */
	Mmouseall = 1<<12,	/* 1003: and all motion */
	Msgr = 1<<13,	/* 1006: mouse reports as SGR */

	/* or'd into a key, above Runemax */
	Vctl = 1<<22,
	Valt = 1<<23,
	Vshift = 1<<24,

	Vgone = 4096,	/* lines kept in Vt.gone a frame; more are lost */
};

struct Vtcell
{
	Rune	r;
	ulong	fg;
	ulong	bg;
	ushort	attr;
	ushort	link;	/* an OSC 8 link's index in Vt.links, or 0 */
};

struct Vtline
{
	Vtcell	*c;	/* Vt.cols of them */
	int	wrapped;	/* the line runs on into the next, by autowrap */
};

/* a scroll: rows top to bot-1 moved up by n, or down by -n */
struct Vtscroll
{
	int	top;
	int	bot;
	int	n;
};

/*
 * What changed since the last frame, in order: the screen was switched or resized (all
 * rows changed), lines left the top of the main screen (Vt.gone), the scrolls happened,
 * then the rows marked in dirty, numbered after the scrolls, changed. Of the lines that left,
 * Vt.gone keeps the last Vgone; lost counts those before them. After vtresize or vtreset the
 * next frame has all set, with the cursor in the screen. scroll, dirty and Vt.gone are
 * libvt's, good until the next vtwrite or vtframe.
 */
struct Vtframe
{
	int	all;
	int	ngone;
	long	lost;
	Vtscroll	*scroll;
	int	nscroll;
	uchar	*dirty;	/* a byte a row */
	int	cursor;	/* the cursor moved, or was shown or hidden */
	int	modes;	/* the modes that changed */
	int	title;	/* the title changed */
	int	bell;	/* times BEL was received */
};

struct Vt
{
	int	cols;
	int	rows;
	Vtline	*line;	/* the screen shown, main or alternate */
	int	x;	/* the cursor: 0 ≤ x < cols, 0 ≤ y < rows */
	int	y;
	int	mode;
	char	*title;	/* OSC 0 and 2 */
	char	**links;	/* OSC 8's URIs, from 1 */
	int	nlinks;
	Vtline	*gone;	/* this frame's lines that left the top, oldest first */

	void	*aux;
	void	(*reply)(Vt*, char*, int);	/* answers to the host (DA, DSR), or nil to drop them */

	/* the parser's state and the other screen are libvt's */
	void	*priv;
};

Vt*	vtnew(int cols, int rows);
Vt*	vtdumb(void (*)(Vt*, Rune*, int));
void	vtfree(Vt*);
void	vtresize(Vt*, int cols, int rows);
void	vtreset(Vt*);
void	vtwrite(Vt*, uchar*, long);
void	vtframe(Vt*, Vtframe*);
int	vtkey(Vt*, int key, char *buf, int n);
long	vtpaste(Vt*, char *s, long len, char *buf, long n);
int	vtmouse(Vt*, int but, int x, int y, int press, char *buf, int n);
int	vtfocus(Vt*, int in, char *buf, int n);
int	vtwidth(Rune);
