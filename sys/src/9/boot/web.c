/*
 * /boot/boot on p9web: boot.c with one method. The kernel has posted the root file server at
 * #s/boot, as boot.c's connectroot does for a server it reached, and made the first process's
 * descriptors (the host's console keeps output and errors apart). What is left is boot's: the
 * user and the system's name from plan9.ini, the name space, then $init or /$cputype/init -t.
 * The file server of /usr keeps a user's directory from here on, as a disk's newuser would.
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
mkdir(char *dir)
{
	int fd;

	if(access(dir, AEXIST) == 0)
		return;
	fd = create(dir, OREAD, DMDIR|0775);
	if(fd < 0)
		fprint(2, "boot: %s: %r\n", dir);
	else
		close(fd);
}

static void
newuser(char *user)
{
	char *s;

	mkdir("#O/usr");
	s = smprint("#O/usr/%s", user);
	mkdir(s);
	mkdir(smprint("%s/tmp", s));
	putenv("HOME", smprint("/usr/%s", user));	/* plan9port's name for $home */
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
	if(user != nil)
		newuser(user);
	execinit(cputype);
}
