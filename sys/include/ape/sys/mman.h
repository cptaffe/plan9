#ifndef __MMAN_H
#define __MMAN_H
#pragma lib "/$M/lib/ape/libap.a"

#include <sys/types.h>

#define	PROT_NONE	0
#define	PROT_READ	1
#define	PROT_WRITE	2
#define	PROT_EXEC	4

#define	MAP_SHARED	0x01
#define	MAP_PRIVATE	0x02
#define	MAP_FIXED	0x10
#define	MAP_ANONYMOUS	0x20
#define	MAP_ANON	MAP_ANONYMOUS

#define	MAP_FAILED	((void*)-1)

#define	MS_ASYNC	1
#define	MS_INVALIDATE	2
#define	MS_SYNC	4

#define	POSIX_MADV_NORMAL	0
#define	POSIX_MADV_RANDOM	1
#define	POSIX_MADV_SEQUENTIAL	2
#define	POSIX_MADV_WILLNEED	3
#define	POSIX_MADV_DONTNEED	4
#ifdef _BSD_EXTENSION
#define	MADV_NORMAL	POSIX_MADV_NORMAL
#define	MADV_RANDOM	POSIX_MADV_RANDOM
#define	MADV_SEQUENTIAL	POSIX_MADV_SEQUENTIAL
#define	MADV_WILLNEED	POSIX_MADV_WILLNEED
#define	MADV_DONTNEED	POSIX_MADV_DONTNEED
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void	*mmap(void*, size_t, int, int, int, off_t);
extern int	munmap(void*, size_t);
extern int	msync(void*, size_t, int);
extern int	mprotect(void*, size_t, int);
extern int	posix_madvise(void*, size_t, int);
#ifdef _BSD_EXTENSION
extern int	madvise(void*, size_t, int);
#endif

#ifdef __cplusplus
}
#endif

#endif
