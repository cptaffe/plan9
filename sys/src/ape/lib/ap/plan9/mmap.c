#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include "lib.h"

typedef struct Map Map;
struct Map
{
	void	*p;
	size_t	n;
	Map	*next;
};

static Map *maps;

void*
mmap(void *addr, size_t n, int prot, int flags, int fd, off_t off)
{
	Map *m;
	char *p;
	ssize_t r;
	size_t got;

	if(n == 0 || off < 0 || (flags&MAP_FIXED) || (flags&(MAP_SHARED|MAP_PRIVATE)) == 0
	|| (flags&(MAP_SHARED|MAP_PRIVATE)) == (MAP_SHARED|MAP_PRIVATE)){
		errno = EINVAL;
		return MAP_FAILED;
	}
	if((flags&MAP_SHARED) && (prot&PROT_WRITE) && !(flags&MAP_ANONYMOUS)){
		errno = ENODEV;
		return MAP_FAILED;
	}
	m = malloc(sizeof *m);
	p = calloc(1, n);
	if(m == nil || p == nil){
		free(m);
		free(p);
		errno = ENOMEM;
		return MAP_FAILED;
	}
	if(!(flags&MAP_ANONYMOUS))
		for(got = 0; got < n; got += r){
			r = pread(fd, p+got, n-got, off+got);
			if(r < 0){
				free(m);
				free(p);
				return MAP_FAILED;
			}
			if(r == 0)
				break;
		}
	m->p = p;
	m->n = n;
	m->next = maps;
	maps = m;
	return p;
}

int
munmap(void *addr, size_t n)
{
	Map *m, **l;

	for(l = &maps; (m = *l) != nil; l = &m->next)
		if(m->p == addr && m->n == n){
			*l = m->next;
			free(m->p);
			free(m);
			return 0;
		}
	errno = EINVAL;
	return -1;
}

int
msync(void *addr, size_t n, int flags)
{
	Map *m;

	for(m = maps; m != nil; m = m->next)
		if((char*)addr >= (char*)m->p && (char*)addr+n <= (char*)m->p+m->n)
			return 0;
	errno = ENOMEM;
	return -1;
}

int
mprotect(void *addr, size_t n, int prot)
{
	(void)addr;
	(void)n;
	(void)prot;
	return 0;
}

int
posix_madvise(void *addr, size_t n, int advice)
{
	(void)addr;
	(void)n;
	if(advice < POSIX_MADV_NORMAL || advice > POSIX_MADV_DONTNEED)
		return EINVAL;
	return 0;
}

int
madvise(void *addr, size_t n, int advice)
{
	if(posix_madvise(addr, n, advice) != 0){
		errno = EINVAL;
		return -1;
	}
	return 0;
}
