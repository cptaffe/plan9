#ifndef __SETJMP_H
#define __SETJMP_H
#pragma lib "/$M/lib/ape/libap.a"

/*
 * As /sys/include/ape/setjmp.h, but for sigsetjmp.  On wasm setjmp is not a
 * function: the compiler rewrites each call it sees in the function that
 * makes it, and a call by way of another function could not come back.  So
 * sigsetjmp must be setjmp's call, here.
 */
typedef int jmp_buf[10];
#ifdef _POSIX_SOURCE
typedef int sigjmp_buf[10];
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int setjmp(jmp_buf);
extern void longjmp(jmp_buf, int);

#ifdef _POSIX_SOURCE
extern long _psigblocked;
#define	sigsetjmp(buf, savemask)	((buf)[0] = (savemask), (buf)[1] = _psigblocked, setjmp(&(buf)[2]))
extern void siglongjmp(sigjmp_buf, int);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SETJMP_H */
