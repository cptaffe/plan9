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
void	scrollinit(Scroll*);
void	scrolldraw(Image*, Scroll*);
void	scrollmouse(Mousectl*, Scroll*, void (*)(Scroll*, vlong, void*), void*);
