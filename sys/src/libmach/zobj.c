#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "zc/z.out.h"
#include "obj.h"

typedef struct Addr	Addr;
struct Addr
{
	char	type;
	char	sym;
	char	name;
};
static Addr addr(Biobuf*);
static char type2char(int);
static void skip(Biobuf*, int);

int
_isz(char *s)
{
	return  s[0] == ANAME
		&& s[1] == D_FILE
		&& s[2] == 1
		&& s[3] == '<';
}

int
_readz(Biobuf *bp, Prog *p)
{
	int as, n;
	Addr a;

	as = Bgetc(bp);
	if(as < 0)
		return 0;
	p->kind = aNone;
	p->sig = 0;
	if(as == ANAME || as == ASIGNAME){
		if(as == ASIGNAME){
			Bread(bp, &p->sig, 4);
			p->sig = leswal(p->sig);
		}
		p->kind = aName;
		p->type = type2char(Bgetc(bp));
		p->sym = Bgetc(bp);
		n = 0;
		for(;;) {
			as = Bgetc(bp);
			if(as < 0)
				return 0;
			n++;
			if(as == 0)
				break;
		}
		p->id = malloc(n);
		if(p->id == 0)
			return 0;
		Bseek(bp, -n, 1);
		if(Bread(bp, p->id, n) != n)
			return 0;
		return 1;
	}
	if(as == ATEXT)
		p->kind = aText;
	else if(as == AGLOBL)
		p->kind = aData;
	skip(bp, 5);
	a = addr(bp);
	addr(bp);
	if(a.type != D_OREG || a.name != D_STATIC && a.name != D_EXTERN)
		p->kind = aNone;
	p->sym = a.sym;
	return 1;
}

static Addr
addr(Biobuf *bp)
{
	Addr a;
	long off;

	a.type = Bgetc(bp);
	skip(bp, 1);
	a.sym = Bgetc(bp);
	a.name = Bgetc(bp);
	switch(a.type){
	default:
	case D_NONE:
	case D_REG:
		break;
	case D_OREG:
	case D_CONST:
	case D_BRANCH:
	case D_WPARAM:
	case D_LOCAL:
		off = Bgetc(bp);
		off |= Bgetc(bp) << 8;
		off |= Bgetc(bp) << 16;
		off |= Bgetc(bp) << 24;
		off = (int)off;
		if(off < 0)
			off = -off;
		if(a.sym && (a.name==D_PARAM || a.name==D_AUTO))
			_offset(a.sym, off);
		break;
	case D_VCONST:
	case D_SCONST:
	case D_FCONST:
		skip(bp, 8);
		break;
	}
	return a;
}

static char
type2char(int t)
{
	switch(t){
	case D_EXTERN:		return 'U';
	case D_STATIC:		return 'b';
	case D_AUTO:		return 'a';
	case D_PARAM:		return 'p';
	default:		return UNKNOWN;
	}
}

static void
skip(Biobuf *bp, int n)
{
	while (n-- > 0)
		Bgetc(bp);
}
