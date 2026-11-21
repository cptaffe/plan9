#include <u.h>
#include <libc.h>
#include <bio.h>
#include "diff.h"

extern int len[2];
extern int *J;

typedef struct Group Group;
struct Group {
	int s;
	int e;
};

typedef struct Score Score;
struct Score {
	int ind;
	int pen;
};

enum {
	Maxblank = 20,
	Maxslide = 100,
};

static char *chg[2];
static short *ind[2];

static int
indent(int k, int l)
{
	if (ind[k][l] == -2)
		ind[k][l] = lineindent(k, l);
	return ind[k][l];
}

static void
split(int k, int l, Score *s)
{
	int i, in, pre, preb, post, postb, b, any;

	in = l > len[k] ? -1 : indent(k, l);
	pre = -1;
	preb = 0;
	for (i = l-1; i >= 1; i--) {
		if ((pre = indent(k, i)) != -1)
			break;
		if (++preb == Maxblank) {
			pre = 0;
			break;
		}
	}
	post = -1;
	postb = 0;
	for (i = l+1; i <= len[k]; i++) {
		if ((post = indent(k, i)) != -1)
			break;
		if (++postb == Maxblank) {
			post = 0;
			break;
		}
	}
	if (pre == -1 && preb == 0)
		s->pen += 1;
	if (l > len[k])
		s->pen += 21;
	b = in == -1 ? 1 + postb : 0;
	any = preb + b != 0;
	s->pen += -30*(preb + b) + 6*b;
	if (in == -1)
		in = post;
	s->ind += in;
	if (in == -1 || pre == -1 || in == pre)
		return;
	if (in > pre)
		s->pen += any ? 10 : -4;
	else if (post != -1 && post > in)
		s->pen += any ? 17 : 24;
	else
		s->pen += any ? 17 : 23;
}

static int
worse(Score *a, Score *b)
{
	return 60*((a->ind > b->ind) - (a->ind < b->ind)) + a->pen - b->pen > 0;
}

static void
first(int k, Group *g)
{
	g->s = g->e = 1;
	while (chg[k][g->e])
		g->e++;
}

static int
next(int k, Group *g)
{
	if (g->e > len[k])
		return -1;
	g->s = g->e+1;
	for (g->e = g->s; chg[k][g->e]; g->e++)
		;
	return 0;
}

static int
prev(int k, Group *g)
{
	if (g->s <= 1)
		return -1;
	g->e = g->s-1;
	for (g->s = g->e; chg[k][g->s-1]; g->s--)
		;
	return 0;
}

static int
down(int k, Group *g)
{
	if (g->e > len[k] || !sameline(k, g->s, g->e))
		return -1;
	chg[k][g->s++] = 0;
	chg[k][g->e++] = 1;
	while (chg[k][g->e])
		g->e++;
	return 0;
}

static int
up(int k, Group *g)
{
	if (g->s <= 1 || !sameline(k, g->s-1, g->e-1))
		return -1;
	chg[k][--g->s] = 1;
	chg[k][--g->e] = 0;
	while (chg[k][g->s-1])
		g->s--;
	return 0;
}

static void
compact(int k)
{
	Group g, o;
	int n, top, align, e, best;
	Score sc, bs;

	first(k, &g);
	first(!k, &o);
	for (;;) {
		if (g.e > g.s) {
			do {
				n = g.e - g.s;
				align = -1;
				while (up(k, &g) == 0)
					prev(!k, &o);
				top = g.e;
				if (o.e > o.s)
					align = g.e;
				while (down(k, &g) == 0) {
					next(!k, &o);
					if (o.e > o.s)
						align = g.e;
				}
			} while (n != g.e - g.s);
			if (g.e != top && align >= 0)
				while (o.e == o.s) {
					up(k, &g);
					prev(!k, &o);
				}
			else if (g.e != top) {
				e = top;
				if (g.e - n - 1 > e)
					e = g.e - n - 1;
				if (g.e - Maxslide > e)
					e = g.e - Maxslide;
				for (best = -1; e <= g.e; e++) {
					sc.ind = sc.pen = 0;
					split(k, e, &sc);
					split(k, e - n, &sc);
					if (best < 0 || !worse(&sc, &bs)) {
						bs = sc;
						best = e;
					}
				}
				while (g.e > best) {
					up(k, &g);
					prev(!k, &o);
				}
			}
		}
		if (next(k, &g) < 0)
			break;
		next(!k, &o);
	}
}

void
slide(void)
{
	int i, j, k;

	for (k = 0; k < 2; k++) {
		chg[k] = MALLOC(char, len[k]+2);
		memset(chg[k], k, len[k]+2);
		chg[k][0] = chg[k][len[k]+1] = 0;
		ind[k] = MALLOC(short, len[k]+2);
		for (i = 0; i <= len[k]+1; i++)
			ind[k][i] = -2;
	}
	for (i = 1; i <= len[0]; i++) {
		chg[0][i] = J[i] == 0;
		if (J[i])
			chg[1][J[i]] = 0;
	}
	compact(0);
	compact(1);
	for (i = j = 1; i <= len[0]; i++) {
		if (chg[0][i]) {
			J[i] = 0;
			continue;
		}
		while (chg[1][j])
			j++;
		J[i] = j++;
	}
	for (k = 0; k < 2; k++) {
		FREE(chg[k]);
		FREE(ind[k]);
	}
}

