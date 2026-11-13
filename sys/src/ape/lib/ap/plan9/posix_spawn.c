/*
 * POSIX's spawn: the child is rforkfn's, which on wasm is a copy of this memory that runs
 * child() from its start, so nothing of this program's stack is copied and a program that
 * spawns needs no asyncify. The program is found and checked here, before the child: a child
 * whose file actions or exec fail exits with status 127, as POSIX allows.
 */
#define _POSIX_C_SOURCE 200809L
#define _BSD_EXTENSION
#include "lib.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sys9.h"

enum {
	Aopen,
	Aclose,
	Adup2,
	Achdir,
	Afchdir,
	Aclosefrom,
};

struct __spawnact {
	int	op;
	int	fd;
	int	fd2;
	int	oflag;
	mode_t	mode;
	char	*path;
};

typedef struct Spawn Spawn;
struct Spawn {
	char	*path;
	char *const	*argv;
	char *const	*envp;
	const posix_spawn_file_actions_t	*fa;
	const posix_spawnattr_t	*at;
};

extern char **environ;

static void
child(void *v)
{
	Spawn *s;
	struct __spawnact *a;
	char dir[1024];
	int i, fd;

	s = v;
	if(s->at){
		if(s->at->__flags & POSIX_SPAWN_SETSID)
			setsid();
		if(s->at->__flags & POSIX_SPAWN_SETPGROUP)
			setpgid(0, s->at->__pgroup);
		if(s->at->__flags & POSIX_SPAWN_SETSIGMASK)
			sigprocmask(SIG_SETMASK, (sigset_t*)&s->at->__mask, NULL);
		if(s->at->__flags & POSIX_SPAWN_SETSIGDEF)
			for(i = 1; i < 32; i++)
				if(sigismember(&s->at->__def, i))
					signal(i, SIG_DFL);
	}
	if(s->fa)
		for(a = s->fa->__a; a < s->fa->__a + s->fa->__n; a++)
			switch(a->op){
			case Aopen:
				if((fd = open(a->path, a->oflag, a->mode)) < 0)
					_exit(127);
				if(fd != a->fd){
					if(dup2(fd, a->fd) < 0)
						_exit(127);
					close(fd);
				}
				break;
			case Aclose:
				close(a->fd);
				break;
			case Adup2:
				if(a->fd == a->fd2)
					fcntl(a->fd, F_SETFD, fcntl(a->fd, F_GETFD) & ~FD_CLOEXEC);
				else if(dup2(a->fd, a->fd2) < 0)
					_exit(127);
				break;
			case Achdir:
				if(chdir(a->path) < 0)
					_exit(127);
				break;
			case Afchdir:
				if(_FD2PATH(a->fd, dir, sizeof dir) < 0 || chdir(dir) < 0)
					_exit(127);
				break;
			case Aclosefrom:
				for(i = a->fd; i < OPEN_MAX; i++)
					if(_fdinfo[i].flags & FD_ISOPEN)
						close(i);
				break;
			}
	execve(s->path, s->argv, s->envp ? s->envp : environ);
	_exit(127);
}

/* found: path's file, or in $PATH's directories if search and it has no '/', and executable */
static char*
found(const char *path, int search, int *err)
{
	const char *p, *e, *d;
	char *f;
	int n;

	if(!search || strchr(path, '/') != NULL){
		if(access(path, X_OK) < 0){
			*err = errno;
			return NULL;
		}
		return strdup(path);
	}
	if((p = getenv("PATH")) == NULL)
		p = "/bin";
	*err = ENOENT;
	for(;; p = e+1){
		e = strchr(p, ':');
		n = e ? e-p : strlen(p);
		d = p;
		if(n == 0){
			d = ".";
			n = 1;
		}
		if((f = malloc(n + 1 + strlen(path) + 1)) == NULL){
			*err = ENOMEM;
			return NULL;
		}
		memcpy(f, d, n);
		f[n] = '/';
		strcpy(f+n+1, path);
		if(access(f, X_OK) == 0)
			return f;
		if(errno != ENOENT)
			*err = errno;
		free(f);
		if(e == NULL)
			return NULL;
	}
}

static int
spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa,
	const posix_spawnattr_t *at, char *const argv[], char *const envp[], int search)
{
	Spawn s;
	int n, err;

	if((s.path = found(path, search, &err)) == NULL)
		return err;
	s.argv = argv;
	s.envp = envp;
	s.fa = fa;
	s.at = at;
	n = rforkfn(RFENVG|RFFDG, child, &s);
	free(s.path);
	if(n < 0)
		return errno;
	if(pid)
		*pid = n;
	return 0;
}

int
posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa,
	const posix_spawnattr_t *at, char *const argv[], char *const envp[])
{
	return spawn(pid, path, fa, at, argv, envp, 0);
}

int
posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fa,
	const posix_spawnattr_t *at, char *const argv[], char *const envp[])
{
	return spawn(pid, file, fa, at, argv, envp, 1);
}

int
posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa)
{
	fa->__n = 0;
	fa->__a = NULL;
	return 0;
}

int
posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	int i;

	for(i = 0; i < fa->__n; i++)
		free(fa->__a[i].path);
	free(fa->__a);
	fa->__n = 0;
	fa->__a = NULL;
	return 0;
}

static int
add(posix_spawn_file_actions_t *fa, int op, int fd, int fd2, const char *path, int oflag, mode_t mode)
{
	struct __spawnact *a;

	if(fd < 0 || fd >= OPEN_MAX || fd2 < 0 || fd2 >= OPEN_MAX)
		return EBADF;
	a = realloc(fa->__a, (fa->__n+1) * sizeof *a);
	if(a == NULL)
		return ENOMEM;
	fa->__a = a;
	a += fa->__n;
	a->op = op;
	a->fd = fd;
	a->fd2 = fd2;
	a->oflag = oflag;
	a->mode = mode;
	a->path = NULL;
	if(path && (a->path = strdup(path)) == NULL)
		return ENOMEM;
	fa->__n++;
	return 0;
}

int
posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *fa, int fd, const char *path, int oflag, mode_t mode)
{
	return add(fa, Aopen, fd, 0, path, oflag, mode);
}

int
posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd)
{
	return add(fa, Aclose, fd, 0, NULL, 0, 0);
}

int
posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int fd2)
{
	return add(fa, Adup2, fd, fd2, NULL, 0, 0);
}

int
posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *fa, const char *path)
{
	return add(fa, Achdir, 0, 0, path, 0, 0);
}

int
posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t *fa, int fd)
{
	return add(fa, Afchdir, fd, 0, NULL, 0, 0);
}

int
posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *fa, const char *path)
{
	return posix_spawn_file_actions_addchdir(fa, path);
}

int
posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t *fa, int fd)
{
	return posix_spawn_file_actions_addfchdir(fa, fd);
}

int
posix_spawn_file_actions_addclosefrom_np(posix_spawn_file_actions_t *fa, int fd)
{
	return add(fa, Aclosefrom, fd, 0, NULL, 0, 0);
}

int
posix_spawnattr_init(posix_spawnattr_t *at)
{
	memset(at, 0, sizeof *at);
	return 0;
}

int
posix_spawnattr_destroy(posix_spawnattr_t *at)
{
	(void)at;
	return 0;
}

int
posix_spawnattr_getflags(const posix_spawnattr_t *at, short *f)
{
	*f = at->__flags;
	return 0;
}

int
posix_spawnattr_setflags(posix_spawnattr_t *at, short f)
{
	at->__flags = f;
	return 0;
}

int
posix_spawnattr_getpgroup(const posix_spawnattr_t *at, pid_t *p)
{
	*p = at->__pgroup;
	return 0;
}

int
posix_spawnattr_setpgroup(posix_spawnattr_t *at, pid_t p)
{
	at->__pgroup = p;
	return 0;
}

int
posix_spawnattr_getsigmask(const posix_spawnattr_t *at, sigset_t *m)
{
	*m = at->__mask;
	return 0;
}

int
posix_spawnattr_setsigmask(posix_spawnattr_t *at, const sigset_t *m)
{
	at->__mask = *m;
	return 0;
}

int
posix_spawnattr_getsigdefault(const posix_spawnattr_t *at, sigset_t *m)
{
	*m = at->__def;
	return 0;
}

int
posix_spawnattr_setsigdefault(posix_spawnattr_t *at, const sigset_t *m)
{
	at->__def = *m;
	return 0;
}
