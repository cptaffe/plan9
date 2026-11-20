typedef struct Test	Test;
typedef struct Inst	Inst;
typedef struct Intern	Intern;
typedef struct Ient	Ient;
typedef struct Tx	Tx;

enum
{
	Tlit = 1,
	Tany,
	Tname,
	Tre,
	Tset,
	Tref,

	Itok = 1,
	Ior,
	Inop,
	Ilbra,
	Irbra,
	Isbeg,
	Isend,
	Iend,
};

struct Test
{
	int	type;
	int	neg;
	int	ref;
	char	*s;
	char	*tag;
	Reprog	*re;
	Test	**set;
	int	nset;
};

struct Inst
{
	int	op;
	int	next;
	int	right;
	Test	*t;
	int	bind;
	int	sub;
	int	star;
	int	*stars;
	int	nstars;
};

struct Tpat
{
	Inst	*i;
	int	ni;
	int	mi;
	int	start;
	int	bstart;
	int	refs;
	int	nstar;
	int	nbind;
	char	*bname[Tmaxcap];
	int	ncap;
	int	capbind[Tmaxcap];
	int	bcap[Tmaxcap];
	int	comments;
	Test	**tests;
	int	ntests;
};

struct Ient
{
	char	*s;
	int	id;
	void	*v;
	Ient	*next;
};

struct Intern
{
	Ient	**h;
	int	nh;
	int	n;
};

struct Tx
{
	Intern	text;
	Intern	names;
	int	*vis;
	int	*rank;
};

uint	_tokhash(char*, int);
Ient*	_tokintern(Intern*, char*, int);
void	_tokinternfree(Intern*);
int	_tokname(char*, char*);
void	_tokview(Toks*, int**, int**);
