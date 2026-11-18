#ifndef __ALLOCA_H
#define __ALLOCA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *alloca(size_t);

#ifdef __cplusplus
}
#endif

#define alloca(n) __builtin_alloca(n)

#endif
