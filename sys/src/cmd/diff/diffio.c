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
static long linelen;
static int eol;
int lines = 3;

#define MAXLINELEN	4096
#define MIN(x, y)	((x) < (y) ? (x): (y))

static int
readline(Biobuf *bp, char *buf)
{
	int c;
	char *p, *e;

	p = buf;
	e = p + MAXLINELEN-1;
	do {
		c = Bgetc(bp);
		if (c < 0) {
			if (p == buf)
				return -1;
			break;
		}
		if (c == '\n')
			break;
		*p++ = c;
	} while (p < e);
	*p = 0;
	linelen = p - buf;
	if (c != '\n' && c >= 0) {
		while ((c = Bgetc(bp)) >= 0 && c != '\n')
			linelen++;
	}
	eol = c == '\n';
	return p - buf;
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
readhash(Biobuf *bp, char *buf)
{
	long sum;
	unsigned shift;
	char *p;
	int len, space;

	sum = 1;
	shift = 0;
	if ((len = readline(bp, buf)) == -1)
		return 0;
	p = buf;
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
	return ((short)low(sum) + (short)high(sum));
}

Biobuf *
prepare(int i, char *arg, char *orig)
{
	struct line *p;
	int j, h;
	Biobuf *bp;
	char *cp, buf[MAXLINELEN];
	int nbytes;
	Rune r;

	bp = Bopen(arg, OREAD);
	if (!bp) {
		panic(mflag ? 0: 2, "cannot open %s: %r\n", arg);
		return 0;
	}
	if (binary)
		return bp;
	nbytes = Bread(bp, buf, MIN(1024, MAXLINELEN));
	if (nbytes > 0) {
		cp = buf;
		while (cp < buf+nbytes-UTFmax) {
			/*
			 * heuristic for a binary file in the
			 * brave new UNICODE world
			 */
			cp += chartorune(&r, cp);
			if (r == 0 || (r > 0x7f && r <= 0xa0)) {
				binary++;
				return bp;
			}
		}
		Bseek(bp, 0, 0);
	}
	p = MALLOC(struct line, 3);
	eol = 1;
	for (j = 0; h = readhash(bp, buf); p[j].value = h)
		p = REALLOC(p, struct line, (++j+3));
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
	long fl, tl;
	char fbuf[MAXLINELEN], tbuf[MAXLINELEN];

	ixold[0] = ixnew[0] = 0;
	for (f = t = 1; f <= len[0]; f++) {
		flen = readline(bf, fbuf);
		fe = eol;
		fl = linelen;
		ixold[f] = ixold[f-1] + linelen + 1;		/* ftell(bf) */
		if (J[f] == 0)
			continue;
		do {
			tlen = readline(bt, tbuf);
			ixnew[t] = ixnew[t-1] + linelen + 1;	/* ftell(bt) */
		} while (t++ < J[f]);
		te = eol;
		tl = linelen;
		if (bflag) {
			flen = squishspace(fbuf);
			tlen = squishspace(tbuf);
		}
		if (flen != tlen || fe != te || !bflag && fl != tl || strcmp(fbuf, tbuf))
			J[f] = 0;
	}
	while (t <= len[1]) {
		tlen = readline(bt, tbuf);
		ixnew[t] = ixnew[t-1] + linelen + 1;	/* fseek(bt) */
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

int
changeset(int i)
{
	int gap;

	gap = 2*lines;
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
		a = changes[i].a-lines;
		b = changes[j-1].b+lines;
		c = changes[i].c-lines;
		d = changes[j-1].d+lines;
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
