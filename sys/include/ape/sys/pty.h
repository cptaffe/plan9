/*
 * Pty support
 */
#ifndef __SYS_PTY_H__
#define __SYS_PTY_H__

#ifndef _BSD_EXTENSION
    This header file is an extension to ANSI/POSIX
#endif

#pragma lib "/$M/lib/ape/libbsd.a"

#ifdef __cplusplus
extern "C" {
#endif
char*	ptsname(int);
char*	ptmname(int);

int	_getpty(void);

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_UIO_H__ */
