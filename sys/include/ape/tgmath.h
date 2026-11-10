#ifndef __TGMATH_H
#define __TGMATH_H
/*
 * C99 tgmath.h, by C11's _Generic: the function is that of the
 * argument's type (of the arguments' sum, when there are more).
 * An integer argument is a double.
 */
#include <math.h>
#include <complex.h>

#define __tg1(x, f, ff, fl)	_Generic((x), float: ff, long double: fl, default: f)
#define __tgc1(x, f, ff, fl, c, cf, cl)	_Generic((x), float: ff, long double: fl, \
	float _Complex: cf, double _Complex: c, long double _Complex: cl, default: f)
/* a zero of the type an argument counts as: an integer's is double's */
#define __tgp(x)	_Generic((x), float: (float)0, long double: (long double)0, \
	float _Complex: (float _Complex)0, double _Complex: (double _Complex)0, \
	long double _Complex: (long double _Complex)0, default: (double)0)
#define __tgs(x, y)	(__tgp(x)+__tgp(y))
#define __tgs3(x, y, z)	(__tgp(x)+__tgp(y)+__tgp(z))

#define acos(x)	__tgc1(x, acos, acosf, acosl, cacos, cacosf, cacosl)(x)
#define asin(x)	__tgc1(x, asin, asinf, asinl, casin, casinf, casinl)(x)
#define atan(x)	__tgc1(x, atan, atanf, atanl, catan, catanf, catanl)(x)
#define acosh(x)	__tgc1(x, acosh, acoshf, acoshl, cacosh, cacoshf, cacoshl)(x)
#define asinh(x)	__tgc1(x, asinh, asinhf, asinhl, casinh, casinhf, casinhl)(x)
#define atanh(x)	__tgc1(x, atanh, atanhf, atanhl, catanh, catanhf, catanhl)(x)
#define cos(x)	__tgc1(x, cos, cosf, cosl, ccos, ccosf, ccosl)(x)
#define sin(x)	__tgc1(x, sin, sinf, sinl, csin, csinf, csinl)(x)
#define tan(x)	__tgc1(x, tan, tanf, tanl, ctan, ctanf, ctanl)(x)
#define cosh(x)	__tgc1(x, cosh, coshf, coshl, ccosh, ccoshf, ccoshl)(x)
#define sinh(x)	__tgc1(x, sinh, sinhf, sinhl, csinh, csinhf, csinhl)(x)
#define tanh(x)	__tgc1(x, tanh, tanhf, tanhl, ctanh, ctanhf, ctanhl)(x)
#define exp(x)	__tgc1(x, exp, expf, expl, cexp, cexpf, cexpl)(x)
#define log(x)	__tgc1(x, log, logf, logl, clog, clogf, clogl)(x)
#define sqrt(x)	__tgc1(x, sqrt, sqrtf, sqrtl, csqrt, csqrtf, csqrtl)(x)
#define fabs(x)	__tgc1(x, fabs, fabsf, fabsl, cabs, cabsf, cabsl)(x)
#define pow(x, y)	__tgc1(__tgs(x, y), pow, powf, powl, cpow, cpowf, cpowl)(x, y)
#define cbrt(x)	__tg1(x, cbrt, cbrtf, cbrtl)(x)
#define ceil(x)	__tg1(x, ceil, ceilf, ceill)(x)
#define erf(x)	__tg1(x, erf, erff, erfl)(x)
#define erfc(x)	__tg1(x, erfc, erfcf, erfcl)(x)
#define exp2(x)	__tg1(x, exp2, exp2f, exp2l)(x)
#define expm1(x)	__tg1(x, expm1, expm1f, expm1l)(x)
#define floor(x)	__tg1(x, floor, floorf, floorl)(x)
#define ilogb(x)	__tg1(x, ilogb, ilogbf, ilogbl)(x)
#define lgamma(x)	__tg1(x, lgamma, lgammaf, lgammal)(x)
#define llrint(x)	__tg1(x, llrint, llrintf, llrintl)(x)
#define llround(x)	__tg1(x, llround, llroundf, llroundl)(x)
#define log10(x)	__tg1(x, log10, log10f, log10l)(x)
#define log1p(x)	__tg1(x, log1p, log1pf, log1pl)(x)
#define log2(x)	__tg1(x, log2, log2f, log2l)(x)
#define logb(x)	__tg1(x, logb, logbf, logbl)(x)
#define lrint(x)	__tg1(x, lrint, lrintf, lrintl)(x)
#define lround(x)	__tg1(x, lround, lroundf, lroundl)(x)
#define nearbyint(x)	__tg1(x, nearbyint, nearbyintf, nearbyintl)(x)
#define rint(x)	__tg1(x, rint, rintf, rintl)(x)
#define round(x)	__tg1(x, round, roundf, roundl)(x)
#define tgamma(x)	__tg1(x, tgamma, tgammaf, tgammal)(x)
#define trunc(x)	__tg1(x, trunc, truncf, truncl)(x)
#define atan2(x, y)	__tg1(__tgs(x, y), atan2, atan2f, atan2l)(x, y)
#define copysign(x, y)	__tg1(__tgs(x, y), copysign, copysignf, copysignl)(x, y)
#define fdim(x, y)	__tg1(__tgs(x, y), fdim, fdimf, fdiml)(x, y)
#define fmax(x, y)	__tg1(__tgs(x, y), fmax, fmaxf, fmaxl)(x, y)
#define fmin(x, y)	__tg1(__tgs(x, y), fmin, fminf, fminl)(x, y)
#define fmod(x, y)	__tg1(__tgs(x, y), fmod, fmodf, fmodl)(x, y)
#define hypot(x, y)	__tg1(__tgs(x, y), hypot, hypotf, hypotl)(x, y)
#define nextafter(x, y)	__tg1(__tgs(x, y), nextafter, nextafterf, nextafterl)(x, y)
#define remainder(x, y)	__tg1(__tgs(x, y), remainder, remainderf, remainderl)(x, y)
#define nexttoward(x, y)	__tg1(x, nexttoward, nexttowardf, nexttowardl)(x, y)
#define fma(x, y, z)	__tg1(__tgs3(x, y, z), fma, fmaf, fmal)(x, y, z)
#define frexp(x, e)	__tg1(x, frexp, frexpf, frexpl)(x, e)
#define ldexp(x, e)	__tg1(x, ldexp, ldexpf, ldexpl)(x, e)
#define scalbn(x, e)	__tg1(x, scalbn, scalbnf, scalbnl)(x, e)
#define scalbln(x, e)	__tg1(x, scalbln, scalblnf, scalblnl)(x, e)
#define remquo(x, y, q)	__tg1(__tgs(x, y), remquo, remquof, remquol)(x, y, q)
#define carg(x)	_Generic((x), float _Complex: cargf, long double _Complex: cargl, float: cargf, long double: cargl, default: carg)(x)
#define cimag(x)	_Generic((x), float _Complex: cimagf, long double _Complex: cimagl, float: cimagf, long double: cimagl, default: cimag)(x)
#define conj(x)	_Generic((x), float _Complex: conjf, long double _Complex: conjl, float: conjf, long double: conjl, default: conj)(x)
#define cproj(x)	_Generic((x), float _Complex: cprojf, long double _Complex: cprojl, float: cprojf, long double: cprojl, default: cproj)(x)
#define creal(x)	_Generic((x), float _Complex: crealf, long double _Complex: creall, float: crealf, long double: creall, default: creal)(x)

#endif /* __TGMATH_H */
