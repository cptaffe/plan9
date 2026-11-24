#ifdef __clang__
#define	P9(name)	__attribute__((import_module("p9web"), import_name(#name)))
#define	EXPORT(name)	__attribute__((export_name(#name)))
#define	wasm·memory_size()	__builtin_wasm_memory_size(0)
#define	wasm·memory_grow(n)	__builtin_wasm_memory_grow(0, n)
#define	wasm·memory_copy(d, s, n)	__builtin_memmove(d, s, n)
#define	wasm·memory_fill(d, c, n)	__builtin_memset(d, c, n)
#define	wasm·unreachable()	__builtin_trap()
#define	wasm·i32_atomic_rmw_add(p, v)	__atomic_fetch_add((int*)(p), (v), __ATOMIC_SEQ_CST)
#define	wasm·i32_atomic_rmw_xchg(p, v)	__atomic_exchange_n((int*)(p), (v), __ATOMIC_SEQ_CST)
#define	wasm·i32_atomic_rmw_cmpxchg(p, o, n)	__sync_val_compare_and_swap((int*)(p), (o), (n))
#else
#define	P9(name)
#define	EXPORT(name)
extern	ulong	wasm·memory_size(void);
extern	ulong	wasm·memory_grow(ulong);
extern	void	wasm·memory_copy(void*, void*, ulong);
extern	void	wasm·memory_fill(void*, int, ulong);
extern	void	wasm·throw(void*);
extern	void	wasm·unreachable(void);
extern	int	wasm·i32_atomic_rmw_add(void*, int);
extern	int	wasm·i32_atomic_rmw_xchg(void*, int);
extern	int	wasm·i32_atomic_rmw_cmpxchg(void*, int, int);
#endif
