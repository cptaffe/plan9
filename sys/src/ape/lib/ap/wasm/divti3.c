#include "ti.h"

static void
neg(W *w)
{
	w->lo = ~w->lo + 1;
	w->hi = ~w->hi + (w->lo == 0);
}

static uvvlong
udivmod(uvvlong a, uvvlong b, uvvlong *rem)
{
	W n, d, q, r;
	int i;

	n.v = a;
	d.v = b;
	if(n.hi == 0 && d.hi == 0){
		q.hi = r.hi = 0;
		q.lo = n.lo / d.lo;
		r.lo = n.lo % d.lo;
	}else{
		q.lo = q.hi = r.lo = r.hi = 0;
		for(i = 127; i >= 0; i--){
			r.hi = r.hi<<1 | r.lo>>63;
			r.lo = r.lo<<1 | ((i >= 64? n.hi>>(i-64): n.lo>>i) & 1);
			if(r.hi > d.hi || (r.hi == d.hi && r.lo >= d.lo)){
				r.hi -= d.hi + (r.lo < d.lo);
				r.lo -= d.lo;
				if(i >= 64)
					q.hi |= 1ULL << (i-64);
				else
					q.lo |= 1ULL << i;
			}
		}
	}
	if(rem)
		*rem = r.v;
	return q.v;
}

uvvlong
__udivti3(uvvlong a, uvvlong b)
{
	return udivmod(a, b, 0);
}

uvvlong
__umodti3(uvvlong a, uvvlong b)
{
	uvvlong r;

	udivmod(a, b, &r);
	return r;
}

vvlong
__divti3(vvlong a, vvlong b)
{
	W n, d, q;
	int s;

	n.v = a;
	d.v = b;
	s = 0;
	if((vlong)n.hi < 0){
		neg(&n);
		s ^= 1;
	}
	if((vlong)d.hi < 0){
		neg(&d);
		s ^= 1;
	}
	q.v = udivmod(n.v, d.v, 0);
	if(s)
		neg(&q);
	return q.v;
}

vvlong
__modti3(vvlong a, vvlong b)
{
	W n, d, r;
	int s;

	n.v = a;
	d.v = b;
	s = (vlong)n.hi < 0;
	if(s)
		neg(&n);
	if((vlong)d.hi < 0)
		neg(&d);
	udivmod(n.v, d.v, &r.v);
	if(s)
		neg(&r);
	return r.v;
}
