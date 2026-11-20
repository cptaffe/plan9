#include "ti.h"

uvvlong
__ashlti3(uvvlong a, int b)
{
	W w;

	w.v = a;
	if(b >= 64){
		w.hi = w.lo << (b-64);
		w.lo = 0;
	}else if(b > 0){
		w.hi = w.hi<<b | w.lo>>(64-b);
		w.lo <<= b;
	}
	return w.v;
}

uvvlong
__lshrti3(uvvlong a, int b)
{
	W w;

	w.v = a;
	if(b >= 64){
		w.lo = w.hi >> (b-64);
		w.hi = 0;
	}else if(b > 0){
		w.lo = w.lo>>b | w.hi<<(64-b);
		w.hi >>= b;
	}
	return w.v;
}

vvlong
__ashrti3(vvlong a, int b)
{
	W w;

	w.v = a;
	if(b >= 64){
		w.lo = (vlong)w.hi >> (b-64);
		w.hi = (vlong)w.hi >> 63;
	}else if(b > 0){
		w.lo = w.lo>>b | w.hi<<(64-b);
		w.hi = (vlong)w.hi >> b;
	}
	return w.v;
}
