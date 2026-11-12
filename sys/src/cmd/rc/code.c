#include "rc.h"
#include "io.h"
#include "exec.h"
#include "fns.h"

#define	c0	t->child[0]
#define	c1	t->child[1]
#define	c2	t->child[2]
int codep, ncode;
#define	emitf(x) ((codep!=ncode || morecode()), codebuf[codep].f = (x), codep++)
#define	emiti(x) ((codep!=ncode || morecode()), codebuf[codep].i = (x), codep++)
#define	emits(x) ((codep!=ncode || morecode()), codebuf[codep].s = (x), codep++)
#define	emitx(x) (mark(), emitf(x))
range emitr;
source *srcs, *csrc;

typedef struct pop pop;
typedef struct loop loop;
struct pop{
	void (*f)(void);
	int n;
	pop *next;
};
struct loop{
	int cont, brk;
	pop *in, *out;
	loop *next;
};
pop *pops, *fnpop;
loop *loops;
void stuffdot(int);
char *fnstr(tree*);
void outcode(tree*, int);
void codeswitch(tree*, int);
int iscase(tree*);
code *codecopy(code*);
void codefree(code*);

int
morecode(void)
{
	ncode+=100;
	codebuf = (code *)realloc((char *)codebuf, ncode*sizeof codebuf[0]);
	if(codebuf==0)
		panic("Can't realloc %d bytes in morecode!",
				ncode*sizeof codebuf[0]);
	return 0;
}

void
stuffdot(int a)
{
	if(a<0 || codep<=a)
		panic("Bad address %d in stuffdot", a);
	codebuf[a].i = codep;
}

static void
mark(void)
{
	source *s = csrc;

	if(s->n%16==0 && (s->at = realloc(s->at, (s->n+16)*sizeof s->at[0]))==0)
		panic("Can't realloc %d bytes in mark!", (s->n+16)*sizeof s->at[0]);
	s->at[s->n].pc = codep;
	s->at[s->n].r = emitr;
	s->n++;
}

static void
freesrc(source *s)
{
	free(s->file);
	free(s->at);
	efree((char *)s);
}

static void
anchor(source *s)
{
	thread *t = runq->ret;
	source *u;
	range *r;

	if(t==0 || (r = srcat(t->code, t->pc-1, &u)) == 0)
		return;
	if(u->own && u->file && strcmp(u->file, flag['m']? flag['m']: Rcmain)==0)
		s->own = 1;
	else if(u->own || u->from.line){
		s->from = u->own? *r: u->from;
		if(u->file)
			s->file = strdup(u->file);
	}
}

int
compile(tree *t)
{
	source *s;

	ncode = 100;
	codebuf = (code *)emalloc(ncode*sizeof codebuf[0]);
	codep = 0;
	s = csrc = new(source);
	memset(s, 0, sizeof *s);
	memset(&emitr, 0, sizeof emitr);
	emiti(0);			/* reference count */
	outcode(t, flag['e']?1:0);
	if(nerror){
		efree((char *)codebuf);
		freesrc(s);
		return 0;
	}
	readhere();
	emitf(Xreturn);
	emitf(0);
	s->code = codebuf;
	if(runq->cmdfile && !runq->iflag){
		s->own = 1;
		s->file = strdup(runq->cmdfile);
	}
	else if(!runq->iflag)
		anchor(s);
	s->next = srcs;
	srcs = s;
	return 1;
}

range*
srcat(code *c, int pc, source **sp)
{
	source *s;
	int i;

	for(s = srcs; s; s = s->next)
		if(s->code==c)
			for(i = 0; i<s->n; i++)
				if(s->at[i].pc==pc && s->at[i].r.line){
					*sp = s;
					return &s->at[i].r;
				}
	return 0;
}

void
cleanhere(char *f)
{
	emitf(Xdelhere);
	emits(strdup(f));
}

char*
fnstr(tree *t)
{
	io *f = openstr();
	void *v;
	extern char nl;
	char svnl = nl;

	nl = ';';
	pfmt(f, "%t", t);
	nl = svnl;
	v = f->strp;
	f->strp = 0;
	closeio(f);
	return v;
}

static void outcode1(tree*, int);

static int
unwind(pop *to)
{
	pop *p;
	int i;

	for(p = pops; p!=to; p = p->next){
		if(p->f==0)
			return 0;
		for(i = 0; i<p->n; i++)
			emitf(p->f);
	}
	return 1;
}

static void
outpop(tree *t, int eflag, void (*f)(void), int n)
{
	pop u;

	u.f = f;
	u.n = n;
	u.next = pops;
	pops = &u;
	outcode(t, eflag);
	pops = u.next;
}

static int
loopbody(tree *t, int eflag, int cont, pop *out)
{
	loop l;

	l.cont = cont;
	l.brk = 0;
	l.in = pops;
	l.out = out;
	l.next = loops;
	loops = &l;
	outcode(t, eflag);
	loops = l.next;
	return l.brk;
}

static void
stuffbrk(int b)
{
	int n;

	for(; b; b = n){
		n = codebuf[b].i;
		codebuf[b].i = codep;
	}
}

static int
isif(tree *t)
{
	return t->type==IF && (c1==0 || c1->type!=ELSE);
}

static void
codeerr(tree *t, char *m)
{
	tokr = t->src;
	yyerror(m);
}

void
outcode(tree *t, int eflag)
{
	range s = emitr;
	pop b;

	if(t==0)
		return;
	if(t->src.line)
		emitr = t->src;
	switch(t->type){
	case '&':
	case '`':
	case SUBSHELL:
	case PIPE:
	case PIPEFD:
		b.f = 0;
		b.n = 0;
		b.next = pops;
		pops = &b;
		outcode1(t, eflag);
		pops = b.next;
		break;
	default:
		outcode1(t, eflag);
	}
	emitr = s;
}

static void
outcode1(tree *t, int eflag)
{
	int p, q, n;
	pop u, v, *fp;
	loop *lp;
	tree *tt;
	char *ifs;

	if(t==0)
		return;
	if(t->type!=NOT && t->type!=';')
		runq->iflast = 0;
	switch(t->type){
	default:
		pfmt(err, "bad type %d in outcode\n", t->type);
		break;
	case '$':
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xdol);
		break;
	case '"':
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xqdol);
		break;
	case SUB:
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xmark);
		outcode(c1, eflag);
		emitx(Xsub);
		break;
	case '&':
		emitx(Xasync);
		if(havefork){
			p = emiti(0);
			outcode(c0, eflag);
			emitf(Xexit);
			stuffdot(p);
		} else
			emits(fnstr(c0));
		break;
	case ';':
		outcode(c0, eflag);
		outcode(c1, eflag);
		break;
	case '^':
		emitf(Xmark);
		outcode(c1, eflag);
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xconc);
		break;
	case '`':
 		emitf(Xmark);
 		if(c0){
 			outcode(c0, 0);
 			emitf(Xglob);
 		} else {
			if((ifs = strdup("ifs")) == nil)
				sysfatal("strdup: %r");
 			emitf(Xmark);
 			emitf(Xword);
 			emits(ifs);
 			emitf(Xdol);
 		}
		emitx(Xbackq);
		if(havefork){
			p = emiti(0);
			outcode(c1, 0);
			emitf(Xexit);
			stuffdot(p);
		} else
			emits(fnstr(c0));
		break;
	case ANDAND:
		outcode(c0, 0);
		emitf(Xtrue);
		p = emiti(0);
		outcode(c1, eflag);
		stuffdot(p);
		break;
	case ARGLIST:
		outcode(c1, eflag);
		outcode(c0, eflag);
		break;
	case BANG:
		outcode(c0, eflag);
		emitf(Xbang);
		break;
	case PCMD:
	case BRACE:
		outcode(c0, eflag);
		break;
	case COUNT:
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xcount);
		break;
	case FN:
		emitf(Xmark);
		outcode(c0, eflag);
		if(c1){
			emitf(Xfn);
			p = emiti(0);
			emits(fnstr(c1));
			u.f = Xunlocal;
			u.n = 1;
			u.next = pops;
			fp = fnpop;
			lp = loops;
			pops = fnpop = &u;
			loops = 0;
			outcode(c1, eflag);
			pops = u.next;
			fnpop = fp;
			loops = lp;
			emitf(Xunlocal);	/* get rid of $* */
			emitf(Xreturn);
			stuffdot(p);
		}
		else
			emitf(Xdelfn);
		break;
	case IF:
		outcode(c0, 0);
		emitf(Xif);
		p = emiti(0);
		if(c1 && c1->type==ELSE){
			outcode(c1->child[0], eflag);
			emitf(Xwastrue);
			emitf(Xjump);
			q = emiti(0);
			stuffdot(p);
			outcode(c1->child[1], eflag);
			stuffdot(q);
			break;
		}
		outcode(c1, eflag);
		emitf(Xwastrue);
		stuffdot(p);
		break;
	case BREAK:
		if(loops==0 || !unwind(loops->out)){
			codeerr(t, "break outside a loop");
			break;
		}
		emitf(Xjump);
		loops->brk = emiti(loops->brk);
		break;
	case CONTINUE:
		if(loops==0 || !unwind(loops->in)){
			codeerr(t, "continue outside a loop");
			break;
		}
		emitf(Xjump);
		emiti(loops->cont);
		break;
	case RETURN:
		if(c0){
			emitf(Xmark);
			outcode(c0, eflag);
			emitf(Xmark);
			emitf(Xword);
			emits(strdup("status"));
			emitx(Xassign);
		}
		if(fnpop==0 || !unwind(fnpop->next)){
			codeerr(t, "return outside a function");
			break;
		}
		emitf(Xreturn);
		break;
	case NOT:
		if(!runq->iflast)
			codeerr(t, "`if not' does not follow `if(...)'");
		emitf(Xifnot);
		p = emiti(0);
		outcode(c0, eflag);
		stuffdot(p);
		break;
	case OROR:
		outcode(c0, 0);
		emitf(Xfalse);
		p = emiti(0);
		outcode(c1, eflag);
		stuffdot(p);
		break;
	case PAREN:
		outcode(c0, eflag);
		break;
	case SIMPLE:
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xsimple);
		if(eflag)
			emitf(Xeflag);
		break;
	case SUBSHELL:
		emitx(Xsubshell);
		if(havefork){
			p = emiti(0);
			outcode(c0, eflag);
			emitf(Xexit);
			stuffdot(p);
		} else
			emits(fnstr(c0));
		if(eflag)
			emitf(Xeflag);
		break;
	case SWITCH:
		codeswitch(t, eflag);
		break;
	case TWIDDLE:
		emitf(Xmark);
		outcode(c1, eflag);
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xmatch);
		if(eflag)
			emitf(Xeflag);
		break;
	case WHILE:
		q = codep;
		outcode(c0, 0);
		if(q==codep)
			emitf(Xsettrue);	/* empty condition == while(true) */
		emitf(Xtrue);
		p = emiti(0);
		n = loopbody(c1, eflag, q, pops);
		emitf(Xjump);
		emiti(q);
		stuffdot(p);
		stuffbrk(n);
		break;
	case WORDS:
		outcode(c1, eflag);
		outcode(c0, eflag);
		break;
	case FOR:
		emitf(Xmark);
		if(c1){
			outcode(c1, eflag);
			emitf(Xglob);
		}
		else{
			emitf(Xmark);
			emitf(Xword);
			emits(strdup("*"));
			emitf(Xdol);
		}
		emitf(Xmark);		/* dummy value for Xlocal */
		emitf(Xmark);
		outcode(c0, eflag);
		emitx(Xlocal);
		p = emitf(Xfor);
		q = emiti(0);
		u.f = Xunlocal;
		u.n = 1;
		u.next = pops;
		v.f = Xpopm;
		v.n = 1;
		v.next = &u;
		pops = &v;
		n = loopbody(c2, eflag, p, u.next);
		pops = u.next;
		emitf(Xjump);
		emiti(p);
		stuffdot(q);
		emitf(Xunlocal);
		stuffbrk(n);
		break;
	case WORD:
		emitf(Xword);
		emits(strdup(t->str));
		break;
	case DUP:
		if(t->rtype==DUPFD){
			emitf(Xdup);
			emiti(t->fd0);
			emiti(t->fd1);
		}
		else{
			emitf(Xclose);
			emiti(t->fd0);
		}
		outpop(c1, eflag, Xpopredir, 1);
		emitf(Xpopredir);
		break;
	case PIPEFD:
		emitx(Xpipefd);
		emiti(t->rtype);
		if(havefork){
			p = emiti(0);
			outcode(c0, eflag);
			emitf(Xexit);
			stuffdot(p);
		} else {
			emits(fnstr(c0));
		}
		break;
	case REDIR:
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xglob);
		switch(t->rtype){
		case APPEND:
			emitx(Xappend);
			break;
		case WRITE:
			emitx(Xwrite);
			break;
		case READ:
		case HERE:
			emitx(Xread);
			break;
		case RDWR:
			emitx(Xrdwr);
			break;
		}
		emiti(t->fd0);
		outpop(c1, eflag, Xpopredir, 1);
		emitf(Xpopredir);
		break;
	case '=':
		tt = t;
		for(;t && t->type=='=';t = c2);
		if(t){					/* var=value cmd */
			n = 0;
			for(t = tt;t->type=='=';t = c2){
				emitr = t->src;
				emitf(Xmark);
				outcode(c1, eflag);
				emitf(Xmark);
				outcode(c0, eflag);
				emitx(Xlocal);		/* push var for cmd */
				n++;
			}
			outpop(t, eflag, Xunlocal, n);	/* gen. code for cmd */
			for(t = tt; t->type == '='; t = c2)
				emitf(Xunlocal);	/* pop var */
		}
		else{					/* var=value */
			for(t = tt;t;t = c2){
				emitr = t->src;
				emitf(Xmark);
				outcode(c1, eflag);
				emitf(Xmark);
				outcode(c0, eflag);
				emitx(Xassign);	/* set var permanently */
			}
		}
		t = tt;	/* so tests below will work */
		break;
	case PIPE:
		emitx(Xpipe);
		emiti(t->fd0);
		emiti(t->fd1);
		if(havefork){
			p = emiti(0);
			q = emiti(0);
			outcode(c0, eflag);
			emitf(Xexit);
			stuffdot(p);
		} else {
			emits(fnstr(c0));
			q = emiti(0);
		}
		outcode(c1, eflag);
		emitf(Xreturn);
		stuffdot(q);
		emitf(Xpipewait);
		break;
	}
	if(t->type!=NOT && t->type!=';')
		runq->iflast = isif(t);
	else if(c0) runq->iflast = isif(c0);
}
/*
 * switch code looks like this:
 *	Xmark
 *	(get switch value)
 *	Xjump	1f
 * out:	Xjump	leave
 * 1:	Xmark
 *	(get case values)
 *	Xcase	1f
 *	(commands)
 *	Xjump	out
 * 1:	Xmark
 *	(get case values)
 *	Xcase	1f
 *	(commands)
 *	Xjump	out
 * 1:
 * leave:
 *	Xpopm
 */

void
codeswitch(tree *t, int eflag)
{
	int leave;		/* patch jump address to leave switch */
	int out;		/* jump here to leave switch */
	int nextcase;	/* patch jump address to next case */
	tree *tt;
	pop u;

	if(c1->child[0]==nil
	|| c1->child[0]->type!=';'
	|| !iscase(c1->child[0]->child[0])){
		codeerr(t, "case missing in switch");
		return;
	}
	emitf(Xmark);
	outcode(c0, eflag);
	emitf(Xjump);
	nextcase = emiti(0);
	out = emitf(Xjump);
	leave = emiti(0);
	stuffdot(nextcase);
	u.f = Xpopm;
	u.n = 1;
	u.next = pops;
	pops = &u;
	t = c1->child[0];
	while(t->type==';'){
		tt = c1;
		emitf(Xmark);
		for(t = c0->child[0];t->type==ARGLIST;t = c0) outcode(c1, eflag);
		emitf(Xcase);
		nextcase = emiti(0);
		t = tt;
		for(;;){
			if(t->type==';'){
				if(iscase(c0)) break;
				outcode(c0, eflag);
				t = c1;
			}
			else{
				if(!iscase(t)) outcode(t, eflag);
				break;
			}
		}
		emitf(Xjump);
		emiti(out);
		stuffdot(nextcase);
	}
	pops = u.next;
	stuffdot(leave);
	emitf(Xpopm);
}

int
iscase(tree *t)
{
	if(t->type!=SIMPLE)
		return 0;
	do t = c0; while(t->type==ARGLIST);
	return t->type==WORD && !t->quoted && strcmp(t->str, "case")==0;
}

code*
codecopy(code *cp)
{
	cp[0].i++;
	return cp;
}

void
codefree(code *cp)
{
	code *p;
	source *s, **l;

	if(--cp[0].i!=0)
		return;
	for(l = &srcs; (s = *l) != 0; l = &s->next)
		if(s->code==cp){
			*l = s->next;
			freesrc(s);
			break;
		}
	for(p = cp+1;p->f;p++){
		if(p->f==Xappend || p->f==Xclose || p->f==Xread || p->f==Xwrite
		|| p->f==Xrdwr
		|| p->f==Xasync || p->f==Xbackq || p->f==Xcase || p->f==Xfalse
		|| p->f==Xfor || p->f==Xif || p->f==Xifnot || p->f==Xjump
		|| p->f==Xsubshell || p->f==Xtrue) p++;
		else if(p->f==Xdup || p->f==Xpipefd) p+=2;
		else if(p->f==Xpipe) p+=4;
		else if(p->f==Xword || p->f==Xdelhere) efree((++p)->s);
		else if(p->f==Xfn){
			efree(p[2].s);
			p+=2;
		}
	}
	efree((char *)cp);
}
