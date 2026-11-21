typedef struct Fsevent Fsevent;
typedef struct Msg Msg;
typedef struct Message Message;
typedef struct Window Window;

enum
{
	STACK		= 8192,
	NPIPEDATA	= 8000,
	NPIPE		= NPIPEDATA+32,
	OUTSIZE	= 16*1024,
	NEVENT		= 5,
};

struct Fsevent
{
	int	type;
	void	*r;
};

struct Msg
{
	Event	e;
	char	*b;
	int	nb;
	int	nr;
};

struct Window
{
	Win	*a;
	int	id;
	Channel	*cevent;
	Msg	m[NEVENT];
};

extern	Window*	wnew(void);
extern	Window*	wold(char*, ulong*);
extern	void		wtag(Window*, char*);
extern	void		wname(Window*, char*);
extern	void		wdump(Window*, char*, char*);
extern	void		wwrite(Window*, Msg*);
extern	int		wbody(Window*, uint, uint, char*);
extern	int		wdel(Window*, int);
extern	void		wevents(void*);
extern	int		waddr(Window*, char*, int);
extern	int		wselect(Window*, char*, int);
extern	void		wzap(Window*, char*);
extern	int		wguard(Window*);
extern	int		wput(Window*, uint, int, char*, int);

extern	void		wctl(Window*, char*, ...);
extern	char*	estrdup(char*);
extern	char*	estrstrdup(char*, char*);
extern	char*	egrow(char*, char*, char*);
extern	char*	eappend(char*, char*, char*);
extern	void		error(char*, ...);

extern	void		startpipe(void);
extern	void		sendit(char*);
extern	void		execevent(Window *w, Msg *m, int (*)(Window*, char*));

extern	void		mountcons(void);
extern	void		fsloop(void*);

extern	int		newpipewin(int, char*);
extern	void		startpipe(void);
extern	int		pipecommand(Window*, char*);
extern	void		pipectl(void*);

#pragma	varargck	argpos	error	1
#pragma	varargck	argpos	wctl	2

extern	Window	*win;
extern	Channel	*fschan, *writechan;
