#pragma	src	"/sys/src/libprop"
#pragma	lib	"libprop.a"

typedef struct Src	Src;
typedef char*	Prop(Src*, void*);

uvlong	choose(Src*, uvlong);
int	coin(Src*, int);
int	pick(Src*, int*, int);
int	more(Src*);
void	span(Src*);
void	endspan(Src*);
void	discard(Src*);
void	note(Src*, char*, ...);

void	propargs(int, char**);
int	forall(char*, Prop*, void*);
void	propexits(void);

Src*	srcbytes(uchar*, long);
char*	runprop(Src*, Prop*, void*);
void	srcfree(Src*);

char*	expand(Src*, char**, char*, int);
char*	mutate(Src*, char**, int);

extern void*	emalloc(ulong);
extern void*	erealloc(void*, ulong);

#pragma	varargck	argpos	note	2
