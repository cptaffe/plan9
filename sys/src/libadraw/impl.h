typedef struct Tagx Tagx;
typedef struct Run Run;
typedef struct Expand Expand;
typedef struct Job Job;
typedef struct Feed Feed;
typedef struct Edit Edit;
typedef struct Dead Dead;
typedef struct Exit Exit;

struct Run
{
	int	pid;
	char	*name;
	Run	*next;
};

struct Expand
{
	uint	q0;
	uint	q1;
	Rune	*name;
	int	nname;
	int	a0;
};

struct Job
{
	int	fd[3];
	char	*cmd;
	char	*dir;
	int	winid;
	char	*file;
	Channel	*c;
	Channel	*errc;
};

struct Dead
{
	Ref	ref;
	Channel	*c;
};

struct Exit
{
	int	pid;
	char	*name;
};

struct Edit
{
	int	op;
	char	*s;
};

struct Feed
{
	int	fd;
	char	*s;
};

struct Tagx
{
	Tag	*tag;
	Mousectl	*mc;
	Keyboardctl	*kc;
	Channel	*wc;
	Channel	*ec;
	Channel	*rz;
	Channel	*rzq;
	Channel	*rzdone;
	Channel	*quit;
	Channel	*donec;
	Dead	*dead;
	QLock	lk;
	Lock	sl;
	char	*snap;
	int	pid;
	int	busy;
	int	owner;
	uint	eq0;
	Rectangle	r;
	Rectangle	all;
	Frame	*f;
	Frame	*g;
	Image	*b;
	Image	*gb;
	Image	*but2;
	Image	*but3;
	Image	*mod;
	Font	*font;
	Rune	*s;
	int	ns;
	uint	q0;
	uint	q1;
	int	dirty;
	int	expand;
	int	winid;
	Point	xy;
	int	clicked;
	ulong	clickmsec;
	Run	*run;
	Rectangle	saved;
	int	filled;
	int	closed;
	int	hangup;
	int	quitting;
	int	nilwant;
	int	nilsent;
};

char*	_adrawstrdup(char*);
char*	_adrawvsmprint(char*, va_list);
char*	_adrawsmprint(char*, ...);
Rune*	_adrawrunesmprint(char*, ...);
char*	_adrawquote(char*);
#pragma	varargck	argpos	_adrawsmprint	1
#pragma	varargck	argpos	_adrawrunesmprint	1
