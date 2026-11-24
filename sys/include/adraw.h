#pragma	lib	"libadraw.a"
#pragma	src	"/sys/src/libadraw"

typedef struct Scroll Scroll;

struct Scroll
{
	Rectangle	r;
	vlong	tot;
	vlong	p0;
	vlong	p1;
	Image	*back;
	Image	*bord;
};

int	scrollwid(void);
int	scrollgap(void);
int	scrollinit(Scroll*);
void	scrolldraw(Image*, Scroll*);
void	scrollmouse(Mousectl*, Scroll*, void (*)(Scroll*, vlong, void*), void*);

typedef struct Tag Tag;
typedef struct Event Event;
typedef struct Keyboardctl Keyboardctl;

struct Tag
{
	Channel	*c;
	Mousectl	*mc;
	Keyboardctl	*kc;
	Rectangle	body;

	struct Tagx	*x;
};

Tag*	tagopen(Mousectl*, Keyboardctl*, char*);
void	tagclose(Tag*);
int	tagctl(Tag*, char*, ...);
#pragma	varargck	argpos	tagctl	2
int	tagwrite(Tag*, char*, void*, int);
char*	tagmread(Tag*, char*);
int	tagreadevent(Tag*, Event*);
int	tagwriteevent(Tag*, Event*);
char*	tagpipe(Tag*, char*, char*);
