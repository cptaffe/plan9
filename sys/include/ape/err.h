#ifndef _BSD_EXTENSION
    This header file is an extension to ANSI/POSIX
#endif

#ifndef __ERR_H_
#define __ERR_H_
#pragma lib "/$M/lib/ape/libbsd.a"

#include <stdarg.h>

/* BSD's: the program's name, the message, and (but for the x forms) strerror of errno, or of code */

#ifdef __cplusplus
extern "C" {
#endif

extern void	err(int, const char *, ...);
extern void	errx(int, const char *, ...);
extern void	errc(int, int, const char *, ...);
extern void	verr(int, const char *, va_list);
extern void	verrx(int, const char *, va_list);
extern void	verrc(int, int, const char *, va_list);
extern void	warn(const char *, ...);
extern void	warnx(const char *, ...);
extern void	warnc(int, const char *, ...);
extern void	vwarn(const char *, va_list);
extern void	vwarnx(const char *, va_list);
extern void	vwarnc(int, const char *, va_list);

#ifdef __cplusplus
}
#endif

#endif
