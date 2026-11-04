/*
 * /boot/boot on p9web: boot.c with one method. The kernel has posted the root file server at
 * #s/boot, as boot.c's connectroot does for a server it reached, and made the first process's
 * descriptors (the host's console keeps output and errors apart). What is left is boot's: the
 * user and the system's name from plan9.ini, the name space, then $init or /$cputype/init -t.
 * The file server of /usr is fossil, on the page's disk (#S): /rc/bin/usrfs starts it, as a
 * disk's boot starts its file server, and /lib/namespace mounts it.
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
	if(user != nil)
		putenv("HOME", smprint("/usr/%s", user));	/* plan9port's name for $home */
	execinit(cputype);
}
