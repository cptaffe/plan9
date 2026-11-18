#ifndef __MATH
#define __MATH
#pragma lib "/$M/lib/ape/libap.a"

/* a HUGE_VAL appropriate for IEEE double-precision */
/* the correct value, 1.797693134862316e+308, causes a ken overflow */
#define HUGE_VAL 1.79769313486231e+308

#ifdef __cplusplus
extern "C" {
#endif

extern double acos(double);
extern double asin(double);
extern double atan(double);
extern double atan2(double, double);
extern double cos(double);
extern double hypot(double, double);
extern double sin(double);
extern double tan(double);
extern double cosh(double);
extern double sinh(double);
extern double tanh(double);
extern double exp(double);
extern double frexp(double, int *);
extern double ldexp(double, int);
extern double log(double);
extern double log10(double);
extern double modf(double, double *);
extern double pow(double, double);
extern double sqrt(double);
extern double ceil(double);
extern double fabs(double);
extern double floor(double);
extern double fmod(double, double);
extern double NaN(void);
extern int isNaN(double);
extern double Inf(int);
extern int isInf(double, int);

#ifdef _RESEARCH_SOURCE
/* does >> treat left operand as unsigned ? */
#define Unsigned_Shifts 1
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

extern double hypot(double, double);
extern double erf(double);
extern double erfc(double);
extern int signgam;	/* lgamma's sign: gamma.c */
extern double j0(double);
extern double y0(double);
extern double j1(double);
extern double y1(double);
extern double jn(int, double);
extern double yn(int, double);

#endif


#ifdef __cplusplus
}
#endif

#if __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L
/*
 * C99: long double is double.  The math functions set errno
 * (wasm keeps no floating-point exceptions); fenv.h's are empty.
 */
typedef float float_t;
typedef double double_t;

#undef HUGE_VAL
#define HUGE_VAL	__builtin_huge_val()
#define HUGE_VALF	__builtin_huge_valf()
#define HUGE_VALL	__builtin_huge_vall()
#define INFINITY	__builtin_inff()
#define NAN	__builtin_nanf("")

#define FP_INFINITE	1
#define FP_NAN	0
#define FP_NORMAL	4
#define FP_SUBNORMAL	3
#define FP_ZERO	2
#define FP_ILOGB0	(-0x7fffffff-1)
#define FP_ILOGBNAN	(-0x7fffffff-1)

#define MATH_ERRNO	1
#define MATH_ERREXCEPT	2
#define math_errhandling	MATH_ERRNO

#define fpclassify(x)	__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x)	__builtin_isfinite(x)
#define isinf(x)	__builtin_isinf(x)
#define isnan(x)	__builtin_isnan(x)
#define isnormal(x)	__builtin_isnormal(x)
#define signbit(x)	__builtin_signbit(x)
#define isgreater(x, y)	__builtin_isgreater(x, y)
#define isgreaterequal(x, y)	__builtin_isgreaterequal(x, y)
#define isless(x, y)	__builtin_isless(x, y)
#define islessequal(x, y)	__builtin_islessequal(x, y)
#define islessgreater(x, y)	__builtin_islessgreater(x, y)
#define isunordered(x, y)	__builtin_isunordered(x, y)

#ifdef __cplusplus
extern "C" {
#endif

extern double acos(double);
extern float acosf(float);
extern long double acosl(long double);
extern double asin(double);
extern float asinf(float);
extern long double asinl(long double);
extern double atan(double);
extern float atanf(float);
extern long double atanl(long double);
extern double cos(double);
extern float cosf(float);
extern long double cosl(long double);
extern double sin(double);
extern float sinf(float);
extern long double sinl(long double);
extern double tan(double);
extern float tanf(float);
extern long double tanl(long double);
extern double acosh(double);
extern float acoshf(float);
extern long double acoshl(long double);
extern double asinh(double);
extern float asinhf(float);
extern long double asinhl(long double);
extern double atanh(double);
extern float atanhf(float);
extern long double atanhl(long double);
extern double cosh(double);
extern float coshf(float);
extern long double coshl(long double);
extern double sinh(double);
extern float sinhf(float);
extern long double sinhl(long double);
extern double tanh(double);
extern float tanhf(float);
extern long double tanhl(long double);
extern double exp(double);
extern float expf(float);
extern long double expl(long double);
extern double exp2(double);
extern float exp2f(float);
extern long double exp2l(long double);
extern double expm1(double);
extern float expm1f(float);
extern long double expm1l(long double);
extern double log(double);
extern float logf(float);
extern long double logl(long double);
extern double log10(double);
extern float log10f(float);
extern long double log10l(long double);
extern double log1p(double);
extern float log1pf(float);
extern long double log1pl(long double);
extern double log2(double);
extern float log2f(float);
extern long double log2l(long double);
extern double logb(double);
extern float logbf(float);
extern long double logbl(long double);
extern double cbrt(double);
extern float cbrtf(float);
extern long double cbrtl(long double);
extern double fabs(double);
extern float fabsf(float);
extern long double fabsl(long double);
extern double sqrt(double);
extern float sqrtf(float);
extern long double sqrtl(long double);
extern double erf(double);
extern float erff(float);
extern long double erfl(long double);
extern double erfc(double);
extern float erfcf(float);
extern long double erfcl(long double);
extern double lgamma(double);
#ifdef _REENTRANT_SOURCE
extern double lgamma_r(double, int *);
#endif
#ifdef _BSD_EXTENSION
extern double gamma(double);	/* lgamma's old name (V7's, gamma.c) */
#endif
extern float lgammaf(float);
extern long double lgammal(long double);
extern double tgamma(double);
extern float tgammaf(float);
extern long double tgammal(long double);
extern double ceil(double);
extern float ceilf(float);
extern long double ceill(long double);
extern double floor(double);
extern float floorf(float);
extern long double floorl(long double);
extern double nearbyint(double);
extern float nearbyintf(float);
extern long double nearbyintl(long double);
extern double rint(double);
extern float rintf(float);
extern long double rintl(long double);
extern double round(double);
extern float roundf(float);
extern long double roundl(long double);
extern double trunc(double);
extern float truncf(float);
extern long double truncl(long double);
extern double atan2(double, double);
extern float atan2f(float, float);
extern long double atan2l(long double, long double);
extern double hypot(double, double);
extern float hypotf(float, float);
extern long double hypotl(long double, long double);
extern double pow(double, double);
extern float powf(float, float);
extern long double powl(long double, long double);
extern double fmod(double, double);
extern float fmodf(float, float);
extern long double fmodl(long double, long double);
extern double remainder(double, double);
extern float remainderf(float, float);
extern long double remainderl(long double, long double);
extern double copysign(double, double);
extern float copysignf(float, float);
extern long double copysignl(long double, long double);
extern double nextafter(double, double);
extern float nextafterf(float, float);
extern long double nextafterl(long double, long double);
extern double fdim(double, double);
extern float fdimf(float, float);
extern long double fdiml(long double, long double);
extern double fmax(double, double);
extern float fmaxf(float, float);
extern long double fmaxl(long double, long double);
extern double fmin(double, double);
extern float fminf(float, float);
extern long double fminl(long double, long double);
extern double frexp(double, int *);
extern float frexpf(float, int *);
extern long double frexpl(long double, int *);
extern double ldexp(double, int);
extern float ldexpf(float, int);
extern long double ldexpl(long double, int);
extern double modf(double, double *);
extern float modff(float, float *);
extern long double modfl(long double, long double *);
extern int ilogb(double);
extern int ilogbf(float);
extern int ilogbl(long double);
extern double scalbn(double, int);
extern float scalbnf(float, int);
extern long double scalbnl(long double, int);
extern double scalbln(double, long);
extern float scalblnf(float, long);
extern long double scalblnl(long double, long);
extern long lrint(double);
extern long lrintf(float);
extern long lrintl(long double);
extern long long llrint(double);
extern long long llrintf(float);
extern long long llrintl(long double);
extern long lround(double);
extern long lroundf(float);
extern long lroundl(long double);
extern long long llround(double);
extern long long llroundf(float);
extern long long llroundl(long double);
extern double remquo(double, double, int *);
extern float remquof(float, float, int *);
extern long double remquol(long double, long double, int *);
extern double nan(const char *);
extern float nanf(const char *);
extern long double nanl(const char *);
extern double nexttoward(double, long double);
extern float nexttowardf(float, long double);
extern long double nexttowardl(long double, long double);
extern double fma(double, double, double);
extern float fmaf(float, float, float);
extern long double fmal(long double, long double, long double);

#ifdef __cplusplus
}
#endif

#else
#define isnan(x) isNaN(x)
#define isinf(x) isInf(x, 0)
#endif

#endif /* __MATH */
