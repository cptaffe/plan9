#ifndef __UCHAR_H
#define __UCHAR_H
#pragma lib "/$M/lib/ape/libap.a"
/*
 * C11 uchar.h: UTF-16 and UTF-32, from and to UTF-8.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
#ifndef _MBSTATE_T
#define _MBSTATE_T
typedef struct { int __n; char __b[4]; unsigned int __c; } mbstate_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern size_t mbrtoc16(char16_t *, const char *, size_t, mbstate_t *);
extern size_t c16rtomb(char *, char16_t, mbstate_t *);
extern size_t mbrtoc32(char32_t *, const char *, size_t, mbstate_t *);
extern size_t c32rtomb(char *, char32_t, mbstate_t *);

#ifdef __cplusplus
}
#endif

#endif /* __UCHAR_H */
