long
ainc(long *p)
{
	return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

long
adec(long *p)
{
	return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

int
tas(int *p)
{
	return __atomic_exchange_n(p, 1, __ATOMIC_SEQ_CST);
}

int
_tas(int *p)
{
	return __atomic_exchange_n(p, 1, __ATOMIC_SEQ_CST);
}

int
cas32(unsigned long *p, unsigned long o, unsigned long n)
{
	return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int
casl(unsigned long *p, unsigned long o, unsigned long n)
{
	return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int
casp(void **p, void *o, void *n)
{
	return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
