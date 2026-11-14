#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include "lib.h"

typedef unsigned int	uint;

enum
{
	MAGIC		= 0xbada110c,
	AMAGIC		= 0xa119ed,	/* aligned_alloc's: next is the bucket's data, pad the size */
	MAX2SIZE	= 32,
	CUTOFF		= 12,
};

typedef struct Bucket Bucket;
struct Bucket
{
	int	size;
	int	magic;
	Bucket	*next;
	int	pad;
	char	data[1];
};

typedef struct Arena Arena;
struct Arena
{
	Bucket	*btab[MAX2SIZE];
};
static Arena arena;

#define datoff		((int)((Bucket*)0)->data)

extern	void	*sbrk(unsigned long);

void*
malloc(size_t size)
{
	uint next;
	int pow, n;
	Bucket *bp, *nbp;

	for(pow = 1; pow < MAX2SIZE; pow++) {
		if(size <= (1<<pow))
			goto good;
	}

	return nil;
good:
	/* Allocate off this list */
	bp = arena.btab[pow];
	if(bp) {
		arena.btab[pow] = bp->next;

		if(bp->magic != 0)
			abort();

		bp->magic = MAGIC;
		return  bp->data;
	}
	size = sizeof(Bucket)+(1<<pow);
	size += 7;
	size &= ~7;

	if(pow < CUTOFF) {
		n = (CUTOFF-pow)+2;
		bp = sbrk(size*n);
		if((intptr_t)bp == -1)
			return nil;

		next = (uint)bp+size;
		nbp = (Bucket*)next;
		arena.btab[pow] = nbp;
		for(n -= 2; n; n--) {
			next = (uint)nbp+size;
			nbp->next = (Bucket*)next;
			nbp->size = pow;
			nbp = nbp->next;
		}
		nbp->size = pow;
	}
	else {
		bp = sbrk(size);
		if((intptr_t)bp == -1)
			return nil;
	}

	bp->size = pow;
	bp->magic = MAGIC;

	return bp->data;
}

void
free(void *ptr)
{
	Bucket *bp, **l;

	if(ptr == nil)
		return;

	/* Find the start of the structure */
	bp = (Bucket*)((uint)ptr - datoff);
	if(bp->magic == AMAGIC){
		bp->magic = 0;
		bp = (Bucket*)((uint)bp->next - datoff);
	}

	if(bp->magic != MAGIC)
		abort();

	bp->magic = 0;
	l = &arena.btab[bp->size];
	bp->next = *l;
	*l = bp;
}

void*
realloc(void *ptr, size_t n)
{
	void *new;
	uint osize;
	Bucket *bp;

	if(ptr == nil)
		return malloc(n);

	/* Find the start of the structure */
	bp = (Bucket*)((uint)ptr - datoff);
	if(bp->magic == AMAGIC){
		new = malloc(n);
		if(new == nil)
			return nil;
		memmove(new, ptr, n < bp->pad? n: bp->pad);
		free(ptr);
		return new;
	}

	if(bp->magic != MAGIC)
		abort();

	/* enough space in this bucket */
	osize = 1<<bp->size;
	if(osize >= n)
		return ptr;

	new = malloc(n);
	if(new == nil)
		return nil;

	memmove(new, ptr, osize);
	free(ptr);

	return new;
}

/*
 * C11: malloc's are 8-aligned; one more aligned is inside one
 * bigger, after a Bucket that says where the bigger is.
 */
void*
aligned_alloc(size_t align, size_t size)
{
	char *p, *q;
	Bucket *bp;

	if(align == 0 || (align & (align-1)) != 0){
		errno = EINVAL;
		return nil;
	}
	if(align <= 8)
		return malloc(size);
	if(size > (size_t)-1 - align - datoff){
		errno = ENOMEM;
		return nil;
	}
	p = malloc(size + align + datoff);
	if(p == nil)
		return nil;
	q = (char*)(((uint)p + datoff + align-1) & ~(align-1));
	bp = (Bucket*)(q - datoff);
	bp->magic = AMAGIC;
	bp->next = (Bucket*)p;
	bp->pad = size;
	return q;
}

int
posix_memalign(void **pp, size_t align, size_t size)
{
	void *p;
	int e;

	if(align < sizeof(void*) || (align & (align-1)) != 0)
		return EINVAL;
	e = errno;
	p = aligned_alloc(align, size);
	if(p == nil){
		errno = e;
		return ENOMEM;
	}
	*pp = p;
	return 0;
}
