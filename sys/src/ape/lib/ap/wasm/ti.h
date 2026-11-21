typedef unsigned long long uvlong;
typedef long long vlong;
typedef __int128 vvlong;
typedef unsigned __int128 uvvlong;

typedef union W W;
union W {
	uvvlong v;
	struct {
		uvlong lo;
		uvlong hi;
	};
};
