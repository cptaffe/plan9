#include <u.h>
#include <libc.h>
#include <bio.h>
#include "diff.h"

static int
itemcmp(void *v1, void *v2)
{
	char **d1 = v1, **d2 = v2;

	return strcmp(*d1, *d2);
}

static char **
scandir(char *name)
{
	char **cp;
	Dir *db;
	int nitems;
	int fd, n;

	if ((fd = open(name, OREAD)) < 0) {
		fprint(2, "%s: can't open %s: %r\n", argv0, name);
		/* fake an empty directory */
		cp = MALLOC(char*, 1);
		cp[0] = 0;
		return cp;
	}
	cp = 0;
	nitems = 0;
	if((n = dirreadall(fd, &db)) > 0){
		while (n--) {
			cp = REALLOC(cp, char *, (nitems+1));
			cp[nitems] = MALLOC(char, strlen((db+n)->name)+1);
			strcpy(cp[nitems], (db+n)->name);
			nitems++;
		}
		free(db);
	}
	cp = REALLOC(cp, char*, (nitems+1));
	cp[nitems] = 0;
	close(fd);
	qsort((char *)cp, nitems, sizeof(char*), itemcmp);
	return cp;
}

static int
isdotordotdot(char *p)
{
	if (*p == '.') {
		if (!p[1])
			return 1;
		if (p[1] == '.' && !p[2])
			return 1;
	}
	return 0;
}

static void
diffnull(char *p, int i, int level)
{
	char **d, **dp, b[MAXPATHLEN+1];
	Dir *sb;

	if ((sb = dirstat(p)) == nil) {
		panic(0, "cannot stat %s: %r\n", p);
		return;
	}
	if (sb->qid.type&QTDIR) {
		d = scandir(p);
		for (dp = d; *dp; dp++) {
			if (rflag && !isdotordotdot(*dp) && mkpathname(b, p, *dp) == 0)
				diffnull(b, i, level+1);
			FREE(*dp);
		}
		FREE(d);
	}
	else if (i == 0)
		diff(p, "/dev/null", level);
	else
		diff("/dev/null", p, level);
	free(sb);
}

void
diffdir(char *f, char *t, int level)
{
	char  **df, **dt, **dirf, **dirt;
	char *from, *to;
	int res;
	char fb[MAXPATHLEN+1], tb[MAXPATHLEN+1];

	df = scandir(f);
	dt = scandir(t);
	dirf = df;
	dirt = dt;
	while (*df || *dt) {
		from = *df;
		to = *dt;
		if (from && isdotordotdot(from)) {
			df++;
			continue;
		}
		if (to && isdotordotdot(to)) {
			dt++;
			continue;
		}
		if (!from)
			res = 1;
		else if (!to)
			res = -1;
		else
			res = strcmp(from, to);
		if (res < 0) {
			if (mode == 0 || mode == 'n')
				Bprint(&stdout, "Only in %s: %s\n", f, from);
			else if (mode == 'u' && mkpathname(fb, f, from) == 0)
				diffnull(fb, 0, level+1);
			df++;
			continue;
		}
		if (res > 0) {
			if (mode == 0 || mode == 'n')
				Bprint(&stdout, "Only in %s: %s\n", t, to);
			else if (mode == 'u' && mkpathname(tb, t, to) == 0)
				diffnull(tb, 1, level+1);
			dt++;
			continue;
		}
		if (mkpathname(fb, f, from))
			continue;
		if (mkpathname(tb, t, to))
			continue;
		diff(fb, tb, level+1);
		df++; dt++;
	}
	for (df = dirf; *df; df++)
		FREE(*df);
	for (dt = dirt; *dt; dt++)
		FREE(*dt);
	FREE(dirf);
	FREE(dirt);
}
