#ifndef __STDINT_H
#define __STDINT_H
/*
 * C99 stdint.h for wasm32: 8 bit chars (signed), 16 bit shorts,
 * 32 bit ints, longs and pointers, 64 bit long longs.
 */

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_fast8_t;
typedef int int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint8_t uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

#ifndef _INTPTR_T
#define _INTPTR_T
typedef long intptr_t;
typedef unsigned long uintptr_t;
#endif

typedef long long intmax_t;
typedef unsigned long long uintmax_t;

#define INT8_MIN	(-0x7f-1)
#define INT16_MIN	(-0x7fff-1)
#define INT32_MIN	(-0x7fffffff-1)
#define INT64_MIN	(-0x7fffffffffffffffLL-1)
#define INT8_MAX	0x7f
#define INT16_MAX	0x7fff
#define INT32_MAX	0x7fffffff
#define INT64_MAX	0x7fffffffffffffffLL
#define UINT8_MAX	0xff
#define UINT16_MAX	0xffff
#define UINT32_MAX	0xffffffffU
#define UINT64_MAX	0xffffffffffffffffULL

#define INT_LEAST8_MIN	INT8_MIN
#define INT_LEAST16_MIN	INT16_MIN
#define INT_LEAST32_MIN	INT32_MIN
#define INT_LEAST64_MIN	INT64_MIN
#define INT_LEAST8_MAX	INT8_MAX
#define INT_LEAST16_MAX	INT16_MAX
#define INT_LEAST32_MAX	INT32_MAX
#define INT_LEAST64_MAX	INT64_MAX
#define UINT_LEAST8_MAX	UINT8_MAX
#define UINT_LEAST16_MAX	UINT16_MAX
#define UINT_LEAST32_MAX	UINT32_MAX
#define UINT_LEAST64_MAX	UINT64_MAX

#define INT_FAST8_MIN	INT8_MIN
#define INT_FAST16_MIN	INT32_MIN
#define INT_FAST32_MIN	INT32_MIN
#define INT_FAST64_MIN	INT64_MIN
#define INT_FAST8_MAX	INT8_MAX
#define INT_FAST16_MAX	INT32_MAX
#define INT_FAST32_MAX	INT32_MAX
#define INT_FAST64_MAX	INT64_MAX
#define UINT_FAST8_MAX	UINT8_MAX
#define UINT_FAST16_MAX	UINT32_MAX
#define UINT_FAST32_MAX	UINT32_MAX
#define UINT_FAST64_MAX	UINT64_MAX

#define INTPTR_MIN	(-0x7fffffffL-1)
#define INTPTR_MAX	0x7fffffffL
#define UINTPTR_MAX	0xffffffffUL
#define INTMAX_MIN	INT64_MIN
#define INTMAX_MAX	INT64_MAX
#define UINTMAX_MAX	UINT64_MAX

#define PTRDIFF_MIN	(-0x7fffffffL-1)
#define PTRDIFF_MAX	0x7fffffffL
#define SIG_ATOMIC_MIN	INT32_MIN
#define SIG_ATOMIC_MAX	INT32_MAX
#define SIZE_MAX	0xffffffffUL
#define WCHAR_MIN	INT32_MIN
#define WCHAR_MAX	INT32_MAX
#define WINT_MIN	0U
#define WINT_MAX	UINT32_MAX

#define INT8_C(c)	c
#define INT16_C(c)	c
#define INT32_C(c)	c
#define INT64_C(c)	c ## LL
#define UINT8_C(c)	c
#define UINT16_C(c)	c
#define UINT32_C(c)	c ## U
#define UINT64_C(c)	c ## ULL
#define INTMAX_C(c)	c ## LL
#define UINTMAX_C(c)	c ## ULL

#endif /* __STDINT_H */
