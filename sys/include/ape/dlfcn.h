#ifndef __DLFCN_H
#define __DLFCN_H
#pragma lib "/$M/lib/ape/libap.a"

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY	1
#define RTLD_NOW	2
#define RTLD_GLOBAL	4
#define RTLD_LOCAL	0

extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int dlclose(void *);
extern char *dlerror(void);

#ifdef __cplusplus
}
#endif

#endif
