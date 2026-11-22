#include <u.h>
#include <libc.h>
#include "wasm.h"

long
ainc(long *p)
{
	return wasm·i32_atomic_rmw_add(p, 1) + 1;
}

long
adec(long *p)
{
	return wasm·i32_atomic_rmw_add(p, -1) - 1;
}

int
_tas(int *p)
{
	return wasm·i32_atomic_rmw_xchg(p, 1);
}

int
cas32(u32int *p, u32int o, u32int n)
{
	return wasm·i32_atomic_rmw_cmpxchg(p, o, n) == o;
}

int
casl(ulong *p, ulong o, ulong n)
{
	return wasm·i32_atomic_rmw_cmpxchg(p, o, n) == o;
}

int
casp(void **p, void *o, void *n)
{
	return wasm·i32_atomic_rmw_cmpxchg(p, (int)o, (int)n) == (int)o;
}
