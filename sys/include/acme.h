#pragma	src	"/sys/src/libacme"
#pragma	lib	"libacme.a"

/*
 * Acme's windows by their files in /mnt/acme (acme(4)), with plan9port's libacme's interface,
 * on open, read and write, through a session of amux(4) when /mnt/amux has the window. The
 * window's files are opened as they are first used and kept. winreadrdevent reads rdevent as
 * winreadevent reads event; winseqopen and winseqwrite are guarded by seq. It needs <u.h>,
 * <libc.h> and <thread.h> first. See acme(2).
 */

typedef struct Event	Event;
typedef struct Win	Win;
typedef struct Winbuf	Winbuf;

#define	EVENTSIZE	256

struct Event
{
	int	c1;
	int	c2;
	int	q0;
	int	q1;
	int	oq0;
	int	oq1;
	int	flag;
	int	nb;
	int	nr;
	char	text[EVENTSIZE*UTFmax+1];
	char	arg[EVENTSIZE*UTFmax+1];
	char	loc[EVENTSIZE*UTFmax+1];
};

struct Winbuf
{
	int	n;
	char	*p;
	char	buf[2048];
};

struct Win
{
	int	id;
	int	clone;
	int	actl;
	char	dir[48];
	int	ctl;
	int	tag;
	int	body;
	int	addr;
	int	event;
	int	data;
	int	xdata;
	int	rdevent;
	Channel	*c;
	QLock	seqlk;

	Winbuf	eb;
	Winbuf	rb;
	Event	e2;
	Event	r2;
};

Win*	newwin(void);
Win*	openwin(int, int);

int	eventfmt(Fmt*);
int	pipewinto(Win*, char*, int, char*, ...);
int	pipetowin(Win*, char*, int, char*, ...);
char*	sysrun(char*, ...);
int	winaddr(Win*, char*, ...);
int	winctl(Win*, char*, ...);
char*	winpath(Win*);
int	windel(Win*, int);
int	winfd(Win*, char*, int);
char*	winmread(Win*, char*);
int	winname(Win*, char*, ...);
int	winprint(Win*, char*, char*, ...);
int	winread(Win*, char*, void*, int);
vlong	winseek(Win*, char*, vlong, int);
int	winreadaddr(Win*, uint*);
int	winreadevent(Win*, Event*);
int	winwrite(Win*, char*, void*, int);
int	winwriteevent(Win*, Event*);
int	winopenfd(Win*, char*, int);
void	winfree(Win*);
void	winclosefiles(Win*);
Channel*	wineventchan(Win*);

int	winopenrdevent(Win*, int);
int	winreadrdevent(Win*, Event*);
int	winseq(Win*);
int	winseqopen(Win*, char*, int, int);
int	winseqwrite(Win*, int, char*, void*, int);

void*	emalloc(ulong);
void*	erealloc(void*, ulong);

#pragma	varargck	argpos	winaddr	2
#pragma	varargck	argpos	winctl	2
#pragma	varargck	argpos	winname	2
#pragma	varargck	argpos	winprint	3
#pragma	varargck	argpos	pipewinto	4
#pragma	varargck	argpos	pipetowin	4
#pragma	varargck	argpos	sysrun	1
