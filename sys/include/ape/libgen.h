#ifndef __LIBGEN_H
#define __LIBGEN_H
#pragma lib "/$M/lib/ape/libap.a"

/* POSIX's libgen: a path's last element, and what is before it. Each may change its argument. */

#ifdef __cplusplus
extern "C" {
#endif

extern char *basename(char *);
extern char *dirname(char *);

#ifdef __cplusplus
}
#endif

#endif
