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

#ifdef __cplusplus
extern "C" {
#endif

extern void	*mmap(void*, size_t, int, int, int, off_t);
extern int	munmap(void*, size_t);
extern int	msync(void*, size_t, int);

#ifdef __cplusplus
}
#endif

#endif
