#include <u.h>
#include <libc.h>
#include <patch.h>

typedef struct Rd	Rd;

struct Rd
{
	char	*p;
	char	*e;
	int	lnum;
};

static char*
peek(Rd *r, int *n)
{
	char *q;

	if(r->p >= r->e)
		return nil;
	q = memchr(r->p, '\n', r->e - r->p);
	*n = q - r->p + 1;
	return r->p;
}

static char*
next(Rd *r, int *n)
{
	char *s;

	if((s = peek(r, n)) == nil)
		return nil;
	r->p += *n;
	r->lnum++;
	return s;
}

static int
prefix(char *s, int n, char *p)
{
	int m;

	m = strlen(p);
	return n >= m && memcmp(s, p, m) == 0;
}

static char*
strdupn(char *s, int n)
{
	char *t;

	t = emalloc(n+1);
	memmove(t, s, n);
	t[n] = 0;
	return t;
}

static int
unquote(char *s, int n, char **out)
{
	char *t, *b;
	int i, c, k;

	b = t = emalloc(n+1);
	for(i = 1; i < n; i++){
		c = s[i];
		if(c == '"'){
			*t = 0;
			*out = b;
			return i+1;
		}
		if(c == '\\' && i+1 < n){
			c = s[++i];
			switch(c){
			case 'a': c = '\a'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'v': c = '\v'; break;
			default:
				if(c >= '0' && c <= '7'){
					for(c = 0, k = 0; k < 3 && i < n && s[i] >= '0' && s[i] <= '7'; k++)
						c = c*8 + s[i++] - '0';
					i--;
				}
			}
		}
		*t++ = c;
	}
	free(b);
	werrstr("unterminated quoted name");
	return -1;
}

static char*
strip(char *s, int p, int lnum)
{
	char *t, *q;

	q = s;
	while(p-- > 0){
		if((t = strchr(q, '/')) == nil)
			break;
		q = t+1;
	}
	if(p >= 0 || *q == 0){
		werrstr("%d: cannot strip a component from %s", lnum, s);
		free(s);
		return nil;
	}
	t = cleanname(strdupn(q, strlen(q)));
	free(s);
	if(*t == '/'){
		werrstr("%d: %s: an absolute name; -p strips its start", lnum, t);
		free(t);
		return nil;
	}
	if(!islocal(t)){
		werrstr("%d: %s: not a local name", lnum, t);
		free(t);
		return nil;
	}
	return t;
}

static int
filename(char *s, int n, char **name)
{
	char *e;

	while(n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
		n--;
	if(n > 0 && s[0] == '"')
		return unquote(s, n, name) < 0 ? -1 : 0;
	if((e = memchr(s, '\t', n)) != nil)
		n = e - s;
	if(n == 0){
		werrstr("missing file name");
		return -1;
	}
	*name = strdupn(s, n);
	return 0;
}

static int
isnull(char *s)
{
	return strcmp(s, "/dev/null") == 0;
}

static int
gitnames(char *s, int n, char **a, char **b)
{
	char *p, *q;
	int i, k, m;

	while(n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
		n--;
	if(n > 0 && s[0] == '"'){
		if((k = unquote(s, n, a)) < 0)
			return -1;
		if(k >= n || s[k] != ' '){
			free(*a);
			werrstr("bad diff --git line");
			return -1;
		}
		if(filename(s+k+1, n-k-1, b) < 0){
			free(*a);
			return -1;
		}
		return 0;
	}
	if((p = memchr(s, '"', n)) != nil && p > s && p[-1] == ' '){
		*a = strdupn(s, p-s-1);
		return unquote(p, n-(p-s), b) < 0 ? -1 : 0;
	}
	for(i = 1; i < n-1; i++){
		if(s[i] != ' ')
			continue;
		m = n-i-1;
		p = memchr(s, '/', i);
		q = memchr(s+i+1, '/', m);
		if(p == nil || q == nil)
			continue;
		k = i - (p+1-s);
		if(k == m - (q+1-(s+i+1)) && memcmp(p+1, q+1, k) == 0){
			*a = strdupn(s, i);
			*b = strdupn(s+i+1, m);
			return 0;
		}
	}
	if((p = memchr(s, ' ', n)) == nil){
		werrstr("bad diff --git line");
		return -1;
	}
	*a = strdupn(s, p-s);
	*b = strdupn(p+1, n-(p-s)-1);
	return 0;
}

static int
mode(char *s, int n, char *p)
{
	char buf[16];

	s += strlen(p);
	n -= strlen(p);
	if(n >= sizeof buf)
		n = sizeof buf-1;
	memmove(buf, s, n);
	buf[n] = 0;
	return strtol(buf, nil, 8);
}

static int
number(char **s, int *v)
{
	char *e;

	*v = strtol(*s, &e, 10);
	if(e == *s)
		return -1;
	*s = e;
	return 0;
}

static int
range(char **s, int *ln, int *cnt)
{
	if(number(s, ln) < 0)
		return -1;
	*cnt = 1;
	if(**s == ','){
		(*s)++;
		if(number(s, cnt) < 0)
			return -1;
	}
	return 0;
}

static int
hunkhdr(char *s, int n, Hunk *h)
{
	char buf[128];

	if(n >= sizeof buf)
		n = sizeof buf-1;
	memmove(buf, s, n);
	buf[n] = 0;
	s = buf+3;
	if(*s++ != '-' || range(&s, &h->oldln, &h->oldcnt) < 0)
		return -1;
	if(*s++ != ' ' || *s++ != '+' || range(&s, &h->newln, &h->newcnt) < 0)
		return -1;
	if(strncmp(s, " @@", 3) != 0)
		return -1;
	if(h->oldln < 0 || h->oldcnt < 0 || h->newln < 0 || h->newcnt < 0)
		return -1;
	return 0;
}

static int
hunk(Rd *r, Hunk *h)
{
	char *s;
	int n, o, w, last, change;
	Line l;

	o = h->oldcnt;
	w = h->newcnt;
	if(o > r->e - r->p || w > r->e - r->p){
		werrstr("%d: hunk longer than the patch", h->lnum);
		return -1;
	}
	h->old.l = emalloc((o+1)*sizeof(Line));
	h->new.l = emalloc((w+1)*sizeof(Line));
	last = 0;
	change = 0;
	while(o > 0 || w > 0){
		if((s = next(r, &n)) == nil){
			werrstr("%d: patch ends inside a hunk", r->lnum);
			return -1;
		}
		l.s = s+1;
		l.n = n-1;
		if(n == 1 && s[0] == '\n'){
			l.s = s;
			l.n = 1;
			last = ' ';
		}else
			last = s[0];
		switch(last){
		case ' ':
			if(o == 0 || w == 0)
				goto Bad;
			h->old.l[h->old.n++] = l;
			h->new.l[h->new.n++] = l;
			o--;
			w--;
			if(change)
				h->trail++;
			else
				h->lead++;
			break;
		case '-':
			if(o == 0)
				goto Bad;
			h->old.l[h->old.n++] = l;
			o--;
			change = 1;
			h->trail = 0;
			break;
		case '+':
			if(w == 0)
				goto Bad;
			h->new.l[h->new.n++] = l;
			w--;
			change = 1;
			h->trail = 0;
			break;
		case '\\':
			werrstr("%d: misplaced \\ line", r->lnum);
			return -1;
		default:
		Bad:
			werrstr("%d: bad line in hunk", r->lnum);
			return -1;
		}
		if((s = peek(r, &n)) != nil && s[0] == '\\'){
			next(r, &n);
			if(last != '+')
				h->old.l[h->old.n-1].n--;
			if(last != '-')
				h->new.l[h->new.n-1].n--;
		}
	}
	if(!change)
		h->trail = 0;
	return 0;
}

static Diff*
newdiff(Patch *pt, int lnum)
{
	Diff *d;

	if(pt->nd % 16 == 0)
		pt->d = erealloc(pt->d, (pt->nd+16)*sizeof(Diff));
	d = &pt->d[pt->nd++];
	memset(d, 0, sizeof *d);
	d->lnum = lnum;
	return d;
}

static int
binary(char *s, int n)
{
	return prefix(s, n, "GIT binary patch") ||
		(prefix(s, n, "Binary files ") || prefix(s, n, "binary files ")) && n > 8 && memcmp(s+n-8, " differ\n", 8) == 0;
}

static int
hunks(Rd *r, Diff *d)
{
	char *s;
	int n;
	Hunk *h;

	while((s = peek(r, &n)) != nil && prefix(s, n, "@@ ")){
		next(r, &n);
		if(d->nh % 16 == 0)
			d->h = erealloc(d->h, (d->nh+16)*sizeof(Hunk));
		h = &d->h[d->nh++];
		memset(h, 0, sizeof *h);
		h->lnum = r->lnum;
		if(hunkhdr(s, n, h) < 0){
			werrstr("%d: bad hunk header", r->lnum);
			return -1;
		}
		if(hunk(r, h) < 0)
			return -1;
	}
	return 0;
}

static int
names(Rd *r, char **a, char **b)
{
	char *s;
	int n;

	s = next(r, &n);
	if(filename(s+4, n-4, a) < 0)
		return -1;
	s = next(r, &n);
	if(filename(s+4, n-4, b) < 0){
		free(*a);
		return -1;
	}
	return 0;
}

static int
isnames(Rd *r)
{
	char *s, *t;
	int n;

	if((s = peek(r, &n)) == nil || !prefix(s, n, "--- "))
		return 0;
	t = s+n;
	if(t >= r->e)
		return 0;
	n = r->e - t;
	return prefix(t, n, "+++ ");
}

static int
gitdiff(Rd *r, Diff *d, char *s, int n, int p)
{
	char *a, *b, *ra, *rb;
	int m;

	d->flags |= Pgit;
	if(p < 0)
		p = 1;
	if(gitnames(s+11, n-11, &a, &b) < 0)
		return -1;
	ra = rb = nil;
	while((s = peek(r, &n)) != nil){
		if(prefix(s, n, "old mode "))
			d->oldmode = mode(s, n, "old mode ");
		else if(prefix(s, n, "new mode "))
			d->newmode = mode(s, n, "new mode ");
		else if(prefix(s, n, "deleted file mode ")){
			d->flags |= Pdel;
			d->oldmode = mode(s, n, "deleted file mode ");
		}else if(prefix(s, n, "new file mode ")){
			d->flags |= Pnew;
			d->newmode = mode(s, n, "new file mode ");
		}else if(prefix(s, n, "rename from ") || prefix(s, n, "copy from ")){
			d->flags |= s[0] == 'r' ? Pren : Pcopy;
			m = s[0] == 'r' ? 12 : 10;
			free(ra);
			if(filename(s+m, n-m, &ra) < 0)
				goto Err;
		}else if(prefix(s, n, "rename to ") || prefix(s, n, "copy to ")){
			d->flags |= s[0] == 'r' ? Pren : Pcopy;
			m = s[0] == 'r' ? 10 : 8;
			free(rb);
			if(filename(s+m, n-m, &rb) < 0)
				goto Err;
		}else if(prefix(s, n, "similarity index ") || prefix(s, n, "dissimilarity index ") || prefix(s, n, "index ")){
		}else if(binary(s, n)){
			werrstr("%d: binary patch", r->lnum+1);
			goto Err;
		}else
			break;
		next(r, &n);
	}
	if(isnames(r)){
		free(a);
		free(b);
		if(names(r, &a, &b) < 0){
			a = b = nil;
			goto Err;
		}
	}
	if(ra != nil || rb != nil){
		if(ra == nil || rb == nil){
			werrstr("%d: rename or copy without both names", d->lnum);
			goto Err;
		}
		free(a);
		free(b);
		a = ra;
		b = rb;
		ra = rb = nil;
		if(p > 0)
			p--;
	}
	if(isnull(a) || d->flags&Pnew){
		free(a);
		a = nil;
		d->flags |= Pnew;
	}
	if(isnull(b) || d->flags&Pdel){
		free(b);
		b = nil;
		d->flags |= Pdel;
	}
	if(a != nil && (d->old = strip(a, p, d->lnum)) == nil || b != nil && (d->new = strip(b, p, d->lnum)) == nil)
		goto Err1;
	a = b = nil;
	if(d->old == nil && d->new == nil){
		werrstr("%d: no file name", d->lnum);
		goto Err1;
	}
	return hunks(r, d);
Err:
	free(a);
	free(b);
Err1:
	free(ra);
	free(rb);
	return -1;
}

static int
plaindiff(Rd *r, Diff *d, int p)
{
	char *a, *b;

	if(p < 0)
		p = 0;
	if(names(r, &a, &b) < 0)
		return -1;
	if(isnull(a)){
		free(a);
		a = nil;
		d->flags |= Pnew;
	}
	if(isnull(b)){
		free(b);
		b = nil;
		d->flags |= Pdel;
	}
	if(a == nil && b == nil){
		werrstr("%d: no file name", d->lnum);
		return -1;
	}
	if(a != nil && (d->old = strip(a, p, d->lnum)) == nil){
		free(b);
		return -1;
	}
	if(b != nil && (d->new = strip(b, p, d->lnum)) == nil)
		return -1;
	return hunks(r, d);
}

static int
check(Diff *d)
{
	int i;
	Hunk *h;

	for(i = 0; i < d->nh; i++){
		h = &d->h[i];
		if(d->flags&Pnew && h->oldcnt != 0){
			werrstr("%d: new file's hunk has old lines", h->lnum);
			return -1;
		}
		if(d->flags&Pdel && h->newcnt != 0){
			werrstr("%d: deleted file's hunk has new lines", h->lnum);
			return -1;
		}
	}
	if(d->flags&Pnew && d->flags&Pdel){
		werrstr("%d: diff of /dev/null and /dev/null", d->lnum);
		return -1;
	}
	return 0;
}

Patch*
parsepatch(char *buf, long n, int p)
{
	Patch *pt;
	Rd r;
	char *s;
	int m;
	Diff *d;

	pt = emalloc(sizeof *pt);
	pt->buf = emalloc(n+2);
	memmove(pt->buf, buf, n);
	if(n > 0 && buf[n-1] != '\n')
		pt->buf[n++] = '\n';
	r.p = pt->buf;
	r.e = r.p+n;
	r.lnum = 0;
	while((s = peek(&r, &m)) != nil){
		if(prefix(s, m, "diff --git ")){
			next(&r, &m);
			d = newdiff(pt, r.lnum);
			if(gitdiff(&r, d, s, m, p) < 0 || check(d) < 0)
				goto Err;
		}else if(isnames(&r)){
			d = newdiff(pt, r.lnum+1);
			if(plaindiff(&r, d, p) < 0 || check(d) < 0)
				goto Err;
		}else if(binary(s, m)){
			werrstr("%d: binary patch", r.lnum+1);
			goto Err;
		}else
			next(&r, &m);
	}
	return pt;
Err:
	freepatch(pt);
	return nil;
}

void
freepatch(Patch *pt)
{
	int i, j;
	Diff *d;

	if(pt == nil)
		return;
	for(i = 0; i < pt->nd; i++){
		d = &pt->d[i];
		for(j = 0; j < d->nh; j++){
			free(d->h[j].old.l);
			free(d->h[j].new.l);
		}
		free(d->h);
		free(d->old);
		free(d->new);
	}
	free(pt->d);
	free(pt->buf);
	free(pt);
}

void
revpatch(Patch *pt)
{
	int i, j, t;
	char *s;
	Text x;
	Diff *d;
	Hunk *h;

	for(i = 0; i < pt->nd; i++){
		d = &pt->d[i];
		s = d->old;
		d->old = d->new;
		d->new = s;
		t = d->oldmode;
		d->oldmode = d->newmode;
		d->newmode = t;
		t = d->flags & ~(Pnew|Pdel);
		if(d->flags&Pnew)
			t |= Pdel;
		if(d->flags&Pdel)
			t |= Pnew;
		d->flags = t;
		for(j = 0; j < d->nh; j++){
			h = &d->h[j];
			t = h->oldln;
			h->oldln = h->newln;
			h->newln = t;
			t = h->oldcnt;
			h->oldcnt = h->newcnt;
			h->newcnt = t;
			x = h->old;
			h->old = h->new;
			h->new = x;
		}
	}
}
