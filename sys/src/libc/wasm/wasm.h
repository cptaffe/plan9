#ifdef __clang__
#define	P9(name)	__attribute__((import_module("p9web"), import_name(#name)))
#define	EXPORT(name)	__attribute__((export_name(#name)))
#define	wasm·memory_size()	__builtin_wasm_memory_size(0)
#define	wasm·memory_grow(n)	__builtin_wasm_memory_grow(0, n)
#define	wasm·memory_copy(d, s, n)	__builtin_memmove(d, s, n)
#define	wasm·memory_fill(d, c, n)	__builtin_memset(d, c, n)
#define	wasm·unreachable()	__builtin_trap()
#else
#define	P9(name)
#define	EXPORT(name)
extern	ulong	wasm·memory_size(void);
extern	ulong	wasm·memory_grow(ulong);
extern	void	wasm·memory_copy(void*, void*, ulong);
extern	void	wasm·memory_fill(void*, int, ulong);
extern	void	wasm·throw(void*);
extern	void	wasm·unreachable(void);
#endif
