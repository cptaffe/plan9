#ifndef __FLOAT
#define __FLOAT
/* IEEE, default rounding */

#define FLT_ROUNDS	1
#define FLT_RADIX	2

#define FLT_DIG		6
#define FLT_EPSILON	1.19209290e-07F
#define FLT_MANT_DIG	24
#define FLT_MAX	3.40282347e+38F
#define FLT_MAX_10_EXP	38
#define FLT_MAX_EXP	128
#define FLT_MIN	1.17549435e-38F
#define FLT_MIN_10_EXP	-37
#define FLT_MIN_EXP	-125

#define DBL_DIG		15
#define DBL_EPSILON	2.2204460492503131e-16
#define DBL_MANT_DIG	53
#define DBL_MAX		1.797693134862315708145e+308
#define DBL_MAX_10_EXP	308
#define DBL_MAX_EXP	1024
#define DBL_MIN		2.225073858507201383090233e-308
#define DBL_MIN_10_EXP	-307
#define DBL_MIN_EXP	-1021
#define LDBL_MANT_DIG	DBL_MANT_DIG
#define LDBL_EPSILON	DBL_EPSILON
#define LDBL_DIG	DBL_DIG
#define LDBL_MIN_EXP	DBL_MIN_EXP
#define LDBL_MIN	DBL_MIN
#define LDBL_MIN_10_EXP	DBL_MIN_10_EXP
#define LDBL_MAX_EXP	DBL_MAX_EXP
#define LDBL_MAX	DBL_MAX
#define LDBL_MAX_10_EXP	DBL_MAX_10_EXP

#if __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L
#define FLT_EVAL_METHOD	0
#define DECIMAL_DIG	17
#endif
#if __STDC_VERSION__ >= 201112L || __cplusplus >= 201703L
#define FLT_DECIMAL_DIG	9
#define DBL_DECIMAL_DIG	17
#define LDBL_DECIMAL_DIG	DBL_DECIMAL_DIG
#define FLT_TRUE_MIN	1.40129846e-45F
#define DBL_TRUE_MIN	4.9406564584124654e-324
#define LDBL_TRUE_MIN	DBL_TRUE_MIN
#define FLT_HAS_SUBNORM	1
#define DBL_HAS_SUBNORM	1
#define LDBL_HAS_SUBNORM	1
#endif

typedef 	union FPdbleword FPdbleword;
union FPdbleword
{
	double	x;
	struct {	/* little endian */
		long lo;
		long hi;
	};
};

#ifdef _RESEARCH_SOURCE
/* define stuff needed for floating conversion */
#define IEEE_8087	1
#endif
#ifdef _PLAN9_SOURCE
/* FCR */
#define	FPINEX	(1<<5)
#define	FPOVFL	(1<<3)
#define	FPUNFL	((1<<4)|(1<<1))
#define	FPZDIV	(1<<2)
#define	FPRNR	(0<<10)
#define	FPRZ	(3<<10)
#define	FPRPINF	(2<<10)
#define	FPRNINF	(1<<10)
#define	FPRMASK	(3<<10)
#define	FPPEXT	(3<<8)
#define	FPPSGL	(0<<8)
#define	FPPDBL	(2<<8)
#define	FPPMASK	(3<<8)
/* FSR */
#define	FPAINEX	FPINEX
#define	FPAOVFL	FPOVFL
#define	FPAUNFL	FPUNFL
#define	FPAZDIV	FPZDIV
#endif
#endif /* __FLOAT */
