#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "diff.h"

struct line {
	int	serial;
	int	value;
};
extern struct line *file[2];
extern int len[2];
extern long *ixold, *ixnew;
extern int *J;

static Biobuf *input[2];
static char *file1, *file2;
static int firstchange;
static int noeol[2];
static int eol;
static int nul;
int lines = 3;

typedef struct Lbuf Lbuf;
struct Lbuf {
	char *s;
	int m;
};

#define MIN(x, y)	((x) < (y) ? (x): (y))

static Lbuf lb[2];

static int
readline(Biobuf *bp, Lbuf *b)
{
	int c, n;

	n = 0;
	while ((c = Bgetc(bp)) >= 0 && c != '\n') {
		if (n+1 >= b->m) {
			b->m = b->m ? 2*b->m : 128;
			b->s = REALLOC(b->s, char, b->m);
		}
		if (c == 0)
			nul = 1;
		b->s[n++] = c;
	}
	if (c < 0 && n == 0)
		return -1;
	if (b->m == 0) {
		b->m = 128;
		b->s = MALLOC(char, b->m);
	}
	b->s[n] = 0;
	eol = c == '\n';
	return n;
}

#define HALFLONG 16
#define low(x)	(x&((1L<<HALFLONG)-1))
#define high(x)	(x>>HALFLONG)

/*
 * hashing has the effect of
 * arranging line in 7-bit bytes and then
 * summing 1-s complement in 16-bit hunks
 */
static int
readhash(Biobuf *bp, Lbuf *b, int *h)
{
	long sum;
	unsigned shift;
	char *p;
	int len, space;

	sum = 1;
	shift = 0;
	if ((len = readline(bp, b)) == -1)
		return -1;
	p = b->s;
	switch(bflag)	/* various types of white space handling */
	{
	case 0:
		while (len--) {
			sum += (long)*p++ << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	case 1:
		/*
		 * coalesce multiple white-space
		 */
		for (space = 0; len--; p++) {
			if (isspace(*p)) {
				space++;
				continue;
			}
			if (space) {
				shift += 7;
				space = 0;
			}
			sum += (long)*p << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	default:
		/*
		 * strip all white-space
		 */
		while (len--) {
			if (isspace(*p)) {
				p++;
				continue;
			}
			sum += (long)*p++ << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	}
	sum = low(sum) + high(sum);
	*h = (short)low(sum) + (short)high(sum);
	if (*h == 0)
		*h = 1;
	return 0;
}

Biobuf *
prepare(int i, char *arg, char *orig)
{
	struct line *p;
	int j, h;
	Biobuf *bp;

	bp = Bopen(arg, OREAD);
	if (!bp) {
		panic(mflag ? 0: 2, "cannot open %s: %r\n", arg);
		return 0;
	}
	if (binary)
		return bp;
	p = MALLOC(struct line, 3);
	eol = 1;
	nul = 0;
	for (j = 0; readhash(bp, &lb[0], &h) >= 0; p[j].value = h)
		p = REALLOC(p, struct line, (++j+3));
	if (nul) {
		binary++;
		FREE(p);
		return bp;
	}
	noeol[i] = !eol;
	if (noeol[i])
		p[j].value ^= 1<<20;
	len[i] = j;
	file[i] = p;
	input[i] = bp;			/*fix*/
	if (i == 0) {			/*fix*/
		file1 = orig;
		firstchange = 0;
	}
	else
		file2 = orig;
	return bp;
}

static int
squishspace(char *buf)
{
	char *p, *q;
	int space;

	for (space = 0, q = p = buf; *q; q++) {
		if (isspace(*q)) {
			space++;
			continue;
		}
		if (space && bflag == 1) {
			*p++ = ' ';
			space = 0;
		}
		*p++ = *q;
	}
	*p = 0;
	return p - buf;
}

/*
 * need to fix up for unexpected EOF's
 */
void
check(Biobuf *bf, Biobuf *bt)
{
	int f, t, flen, tlen, fe, te;

	ixold[0] = ixnew[0] = 0;
	for (f = t = 1; f <= len[0]; f++) {
		flen = readline(bf, &lb[0]);
		fe = eol;
		ixold[f] = ixold[f-1] + flen + 1;		/* ftell(bf) */
		if (J[f] == 0)
			continue;
		do {
			tlen = readline(bt, &lb[1]);
			ixnew[t] = ixnew[t-1] + tlen + 1;	/* ftell(bt) */
		} while (t++ < J[f]);
		te = eol;
		if (bflag) {
			flen = squishspace(lb[0].s);
			tlen = squishspace(lb[1].s);
		}
		if (flen != tlen || fe != te || memcmp(lb[0].s, lb[1].s, flen))
			J[f] = 0;
	}
	while (t <= len[1]) {
		tlen = readline(bt, &lb[1]);
		ixnew[t] = ixnew[t-1] + tlen + 1;	/* fseek(bt) */
		t++;
	}
}

int
sameline(int k, int a, int b)
{
	long *ix, n, o, m;
	char x[512], y[512];

	ix = k ? ixnew : ixold;
	n = ix[a] - ix[a-1];
	if (n != ix[b] - ix[b-1] || noeol[k] && (a == len[k]) != (b == len[k]))
		return 0;
	for (o = 0; o < n; o += m) {
		m = n - o;
		if (m > sizeof x)
			m = sizeof x;
		Bseek(input[k], ix[a-1]+o, 0);
		if (Bread(input[k], x, m) != m)
			return 0;
		Bseek(input[k], ix[b-1]+o, 0);
		if (Bread(input[k], y, m) != m)
			return 0;
		if (memcmp(x, y, m) != 0)
			return 0;
	}
	return 1;
}

int
lineindent(int k, int l)
{
	long *ix, n, o;
	int c, ind;

	ix = k ? ixnew : ixold;
	n = ix[l] - ix[l-1];
	Bseek(input[k], ix[l-1], 0);
	ind = 0;
	for (o = 0; o < n && (c = Bgetc(input[k])) >= 0; o++) {
		if (!isspace(c))
			return ind;
		if (c == ' ')
			ind++;
		else if (c == '\t')
			ind += 8 - ind%8;
		if (ind >= 200)
			return 200;
	}
	return -1;
}

static void
range(int a, int b, char *separator)
{
	Bprint(&stdout, "%d", a > b ? b: a);
	if (a < b)
		Bprint(&stdout, "%s%d", separator, b);
}

static void
fetch(long *f, int a, int b, Biobuf *bp, char *s)
{
	int c, i, maxb;

	if(a <= 1)
		a = 1;
	i = bp != input[0];
	maxb = len[i];
	if(b > maxb)
		b = maxb;
	if(a > maxb)
		return;
	Bseek(bp, f[a-1], 0);
	for (; a <= b; a++) {
		Bprint(&stdout, "%s", s);
		while ((c = Bgetc(bp)) >= 0 && c != '\n')
			Bputc(&stdout, c);
		Bputc(&stdout, '\n');
		if (a == maxb && noeol[i] && mode != 'e' && mode != 'f')
			Bprint(&stdout, "\\ No newline at end of file\n");
	}
}

typedef struct Change Change;
struct Change
{
	int a;
	int b;
	int c;
	int d;
};

Change *changes;
int nchanges;

void
change(int a, int b, int c, int d)
{
	char verb;
	char buf[4];
	Change *ch;

	if (a > b && c > d)
		return;
	anychange = 1;
	if (mflag && firstchange == 0) {
		if(mode) {
			buf[0] = '-';
			buf[1] = mode;
			buf[2] = ' ';
			buf[3] = '\0';
		} else {
			buf[0] = '\0';
		}
		Bprint(&stdout, "diff %s%s %s\n", buf, file1, file2);
		firstchange = 1;
	}
	verb = a > b ? 'a': c > d ? 'd': 'c';
	switch(mode) {
	case 'e':
		range(a, b, ",");
		Bputc(&stdout, verb);
		break;
	case 0:
		range(a, b, ",");
		Bputc(&stdout, verb);
		range(c, d, ",");
		break;
	case 'n':
		Bprint(&stdout, "%s:", file1);
		range(a, b, ",");
		Bprint(&stdout, " %c ", verb);
		Bprint(&stdout, "%s:", file2);
		range(c, d, ",");
		break;
	case 'f':
		Bputc(&stdout, verb);
		range(a, b, " ");
		break;
	case 'c':
	case 'a':
	case 'u':
		if(nchanges%1024 == 0)
			changes = erealloc(changes, (nchanges+1024)*sizeof(changes[0]));
		ch = &changes[nchanges++];
		ch->a = a;
		ch->b = b;
		ch->c = c;
		ch->d = d;
		return;
	}
	Bputc(&stdout, '\n');
	if (mode == 0 || mode == 'n') {
		fetch(ixold, a, b, input[0], "< ");
		if (a <= b && c <= d)
			Bprint(&stdout, "---\n");
	}
	fetch(ixnew, c, d, input[1], mode == 0 || mode == 'n' ? "> ": "");
	if (mode != 0 && mode != 'n' && c <= d)
		Bprint(&stdout, ".\n");
}

static int
context(void)
{
	if (lines > len[0] + len[1])
		return len[0] + len[1];
	return lines;
}

int
changeset(int i)
{
	int gap;

	gap = 2*context();
	if(mode == 'u')
		gap++;
	while(i+1<nchanges && changes[i].b+1+gap > changes[i+1].a)
		i++;
	if(i<nchanges)
		return i+1;
	return nchanges;
}

static void
urange(int a, int b)
{
	if(b < a)
		Bprint(&stdout, "%d,0", b);
	else if(a == b)
		Bprint(&stdout, "%d", a);
	else
		Bprint(&stdout, "%d,%d", a, b-a+1);
}

void
flushchanges(void)
{
	int a, b, c, d, at;
	int i, j;

	if(nchanges == 0)
		return;

	if(mode == 'u')
		Bprint(&stdout, "--- %s\n+++ %s\n", file1, file2);
	for(i=0; i<nchanges; ){
		j = changeset(i);
		a = changes[i].a-context();
		b = changes[j-1].b+context();
		c = changes[i].c-context();
		d = changes[j-1].d+context();
		if(a < 1)
			a = 1;
		if(c < 1)
			c = 1;
		if(b > len[0])
			b = len[0];
		if(d > len[1])
			d = len[1];
		if(mode == 'a'){
			a = 1;
			b = len[0];
			c = 1;
			d = len[1];
			j = nchanges;
		}
		if(mode == 'u'){
			Bprint(&stdout, "@@ -");
			urange(a, b);
			Bprint(&stdout, " +");
			urange(c, d);
			Bprint(&stdout, " @@\n");
		}else{
			Bprint(&stdout, "%s:", file1);
			range(a, b, ",");
			Bprint(&stdout, " - ");
			Bprint(&stdout, "%s:", file2);
			range(c, d, ",");
			Bputc(&stdout, '\n');
		}
		at = a;
		for(; i<j; i++){
			fetch(ixold, at, changes[i].a-1, input[0], mode == 'u' ? " " : "  ");
			fetch(ixold, changes[i].a, changes[i].b, input[0], mode == 'u' ? "-" : "- ");
			fetch(ixnew, changes[i].c, changes[i].d, input[1], mode == 'u' ? "+" : "+ ");
			at = changes[i].b+1;
		}
		fetch(ixold, at, b, input[0], mode == 'u' ? " " : "  ");
	}
	nchanges = 0;
}
