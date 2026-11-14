/*
 * /boot/boot on p9web: boot.c with one method. The kernel has posted the root file server at
 * #s/boot, as boot.c's connectroot does for a server it reached, and made the first process's
 * descriptors (the host's console keeps output and errors apart). What is left is boot's: the
 * user and the system's name from plan9.ini, the name space, then $init or /$cputype/init -t.
 * The file server of /usr is fossil, on the page's disk (#S): /rc/bin/usrfs starts it, as a
 * disk's boot starts its file server, and /lib/namespace mounts it. Then factotum, as bootauth.c
 * starts it for a terminal.
 */
#include <u.h>
#include <libc.h>

enum
{
	Nargv = 1024,
};

static char rootdir[] = "/root";
static char *iargv[Nargv];

static void
fatal(char *s)
{
	fprint(2, "boot: %s: %r\n", s);
	exits(s);
}

static void
writefile(char *name, char *s)
{
	int fd;

	fd = open(name, OWRITE);
	if(fd < 0 || write(fd, s, strlen(s)) < 0)
		fprint(2, "boot: %s: %r\n", name);
	if(fd >= 0)
		close(fd);
}

/* init will reinitialize its namespace. #ec gets us plan9.ini settings */
static void
bindenvsrv(void)
{
	bind("#c", "/dev", MREPL);
	bind("#N", "/dev", MAFTER);
	bind("#ec", "/env", MREPL);
	bind("#e", "/env", MBEFORE|MCREATE);
	bind("#s", "/srv/", MREPL|MCREATE);
}

static void
usrfs(char *cputype)
{
	int pid;
	Waitmsg *w;

	if(access("#S/sdW0/data", AEXIST) < 0 || access("/rc/bin/usrfs", AEXIST) < 0)
		return;	/* no disk, or a root with no file server for it */
	switch(pid = fork()){
	case -1:
		fprint(2, "boot: fork: %r\n");
		return;
	case 0:
		execl(smprint("/%s/bin/rc", cputype), "rc", "/rc/bin/usrfs", nil);
		fatal("exec rc");
	}
	while((w = wait()) != nil && w->pid != pid)
		free(w);
	if(w != nil && w->msg[0])
		fprint(2, "boot: usrfs: %s\n", w->msg);
	free(w);
}

/*
 * authentication is bootauth.c's for a terminal whose user plan9.ini names: factotum, posted
 * as #s/factotum (which /lib/namespace mounts on /mnt), then its keys. A terminal's would come
 * from secstore; here they are the disk's keys partition, which only the hostowner reads.
 */
static void
authentication(char *cputype)
{
	char *av[8], *s, *b, *p, *q;
	int ac, fd, n, pid;
	Waitmsg *w;
	Dir *d;

	s = smprint("/%s/bin/auth/factotum", cputype);
	if(access(s, AEXEC) < 0){
		free(s);
		return;
	}
	ac = 0;
	av[ac++] = "factotum";
	if(getenv("debugfactotum"))
		av[ac++] = "-p";
	p = getenv("factotumopts");
	if(p != nil && *p != '\0')
		av[ac++] = p;
	av[ac++] = "-sfactotum";
	av[ac] = nil;
	switch(pid = fork()){
	case -1:
		fatal("starting factotum");
	case 0:
		exec(s, av);
		fatal("execing factotum");
	}
	free(s);
	/*
	 * factotum's first process ends when its server is posted and mounted: wait for it, where
	 * bootauth.c polls for /mnt/factotum, so that its exit is not a wait record for the process
	 * this one becomes (init, or a $init program that waits for its own children).
	 */
	while((w = wait()) != nil && w->pid != pid)
		free(w);
	if(w != nil && w->msg[0])
		fprint(2, "boot: factotum: %s\n", w->msg);
	free(w);

	/* the keys: one a line, as factotum's ctl takes them */
	fd = open("#S/sdW0/keys", OREAD);
	if(fd < 0)
		return;
	d = dirfstat(fd);
	if(d == nil || d->length == 0){
		free(d);
		close(fd);
		return;
	}
	b = malloc(d->length+1);
	n = b == nil ? -1 : readn(fd, b, d->length);
	free(d);
	close(fd);
	if(n <= 0){
		free(b);
		return;
	}
	b[n] = 0;
	fd = open("/mnt/factotum/ctl", OWRITE);
	if(fd < 0){
		fprint(2, "boot: /mnt/factotum/ctl: %r\n");
		free(b);
		return;
	}
	for(p = b; p != nil && *p != 0; p = q){
		q = strchr(p, '\n');
		if(q != nil)
			*q++ = 0;
		if(*p != 0 && *p != '#' && write(fd, p, strlen(p)) < 0)
			fprint(2, "boot: factotum key: %r\n");
	}
	memset(b, 0, n);
	free(b);
	close(fd);
}

static void
nsinit(int fd)
{
	char *rp;

	if(bind("/", "/", MREPL) < 0)
		fatal("bind /");
	rp = getenv("rootspec");
	if(rp == nil)
		rp = "";
	if(mount(fd, -1, "/root", MREPL|MCREATE, rp) < 0)
		fatal("mount /");
	rp = getenv("rootdir");
	if(rp == nil)
		rp = rootdir;
	if(bind(rp, "/", MAFTER|MCREATE) < 0)
		fatal("second bind /");
	putenv("rootdir", rp);
}

static void
execinit(char *cputype)
{
	int iargc;
	char *cmd;

	cmd = getenv("init");
	if(cmd == nil)
		cmd = smprint("/%s/init -t", cputype);
	iargc = tokenize(cmd, iargv, nelem(iargv)-1);
	cmd = iargv[0];

	/* make iargv[0] basename(iargv[0]) */
	if(iargv[0] = strrchr(iargv[0], '/'))
		iargv[0]++;
	else
		iargv[0] = cmd;
	iargv[iargc] = nil;

	exec(cmd, iargv);
	fatal(cmd);
}

void
main(int, char**)
{
	int fd;
	char *user, *sysname, *cputype;

	bindenvsrv();
	cputype = getenv("cputype");
	if(cputype == nil)
		fatal("no $cputype");
	user = getenv("user");
	if(user != nil)
		writefile("#c/hostowner", user);	/* and the caller's */
	sysname = getenv("sysname");
	if(sysname != nil)
		writefile("#c/sysname", sysname);
	rfork(RFNAMEG);
	fd = open("#s/boot", ORDWR);
	if(fd < 0)
		fatal("open #s/boot");
	nsinit(fd);
	close(fd);
	usrfs(cputype);
	authentication(cputype);
	if(user != nil)
		putenv("HOME", smprint("/usr/%s", user));	/* plan9port's name for $home */
	execinit(cputype);
}
