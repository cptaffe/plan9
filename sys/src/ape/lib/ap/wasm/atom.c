/* One process in its memory, and a switch only at a call: no atomic instruction is needed. */
long ainc(long *p) { return ++*p; }
long adec(long *p) { return --*p; }

int
_tas(int *p)
{
	int v;

	v = *p;
	*p = 1;
	return v;
}

int
cas32(unsigned long *p, unsigned long o, unsigned long n)
{
	if(*p != o)
		return 0;
	*p = n;
	return 1;
}

int casl(unsigned long *p, unsigned long o, unsigned long n) { return cas32(p, o, n); }
int casp(void **p, void *o, void *n) { return cas32((unsigned long*)p, (unsigned long)o, (unsigned long)n); }
