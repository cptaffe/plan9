#ifndef __SPAWN_H
#define __SPAWN_H
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200112L
   This header file is not defined in pure ANSI or POSIX before 2001
#endif
#pragma lib "/$M/lib/ape/libap.a"

/* POSIX's spawn: a child that runs another program, made without copying this one. There is
 * no process scheduling, so no POSIX_SPAWN_SETSCHEDPARAM or POSIX_SPAWN_SETSCHEDULER. */

#include <sys/types.h>
#include <signal.h>

#define	POSIX_SPAWN_RESETIDS	0x01
#define	POSIX_SPAWN_SETPGROUP	0x02
#define	POSIX_SPAWN_SETSIGDEF	0x04
#define	POSIX_SPAWN_SETSIGMASK	0x08
#define	POSIX_SPAWN_SETSID	0x80

typedef struct {
	short	__flags;
	pid_t	__pgroup;
	sigset_t	__mask;
	sigset_t	__def;
} posix_spawnattr_t;

typedef struct {
	int	__n;
	struct __spawnact	*__a;
} posix_spawn_file_actions_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int	posix_spawn(pid_t *, const char *, const posix_spawn_file_actions_t *,
			const posix_spawnattr_t *, char *const [], char *const []);
extern int	posix_spawnp(pid_t *, const char *, const posix_spawn_file_actions_t *,
			const posix_spawnattr_t *, char *const [], char *const []);

extern int	posix_spawn_file_actions_init(posix_spawn_file_actions_t *);
extern int	posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *);
extern int	posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *, int, const char *, int, mode_t);
extern int	posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *, int);
extern int	posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *, int, int);
extern int	posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *, const char *);
extern int	posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t *, int);
#ifdef _BSD_EXTENSION
extern int	posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *, const char *);
extern int	posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t *, int);
extern int	posix_spawn_file_actions_addclosefrom_np(posix_spawn_file_actions_t *, int);
#endif

extern int	posix_spawnattr_init(posix_spawnattr_t *);
extern int	posix_spawnattr_destroy(posix_spawnattr_t *);
extern int	posix_spawnattr_getflags(const posix_spawnattr_t *, short *);
extern int	posix_spawnattr_setflags(posix_spawnattr_t *, short);
extern int	posix_spawnattr_getpgroup(const posix_spawnattr_t *, pid_t *);
extern int	posix_spawnattr_setpgroup(posix_spawnattr_t *, pid_t);
extern int	posix_spawnattr_getsigmask(const posix_spawnattr_t *, sigset_t *);
extern int	posix_spawnattr_setsigmask(posix_spawnattr_t *, const sigset_t *);
extern int	posix_spawnattr_getsigdefault(const posix_spawnattr_t *, sigset_t *);
extern int	posix_spawnattr_setsigdefault(posix_spawnattr_t *, const sigset_t *);

#ifdef __cplusplus
}
#endif

#endif
