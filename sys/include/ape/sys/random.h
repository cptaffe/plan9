#ifndef __SYS_RANDOM_H
#define __SYS_RANDOM_H
#pragma lib "/$M/lib/ape/libap.a"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int getentropy(void *, size_t);

#ifdef __cplusplus
}
#endif

#endif
