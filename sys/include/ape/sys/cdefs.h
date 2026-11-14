#ifndef __SYS_CDEFS_H
#define __SYS_CDEFS_H

#ifdef __cplusplus
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif

#define	__P(x)	x
#define	__CONCAT(x, y)	x ## y
#define	__STRING(x)	#x

#define	__dead	__attribute__((__noreturn__))
#define	__dead2	__attribute__((__noreturn__))
#define	__pure	__attribute__((__pure__))
#define	__packed	__attribute__((__packed__))
#define	__aligned(x)	__attribute__((__aligned__(x)))
#define	__printflike(f, a)	__attribute__((__format__(__printf__, f, a)))
#define	__scanflike(f, a)	__attribute__((__format__(__scanf__, f, a)))
#ifndef __unused
#define	__unused	__attribute__((__unused__))
#endif
#ifndef __used
#define	__used	__attribute__((__used__))
#endif

#define	__RCSID(s)	struct __hack
#define	__FBSDID(s)	struct __hack
#define	__COPYRIGHT(s)	struct __hack
#define	__SCCSID(s)	struct __hack

#endif
