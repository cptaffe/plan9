#pragma lib "/$M/lib/ape/libap.a"

#undef assert
#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#ifdef __cplusplus
extern "C" {
#endif

extern void _assert(char *, unsigned);

#ifdef __cplusplus
}
#endif
#define assert(e) ((e)? (void)0: _assert(__FILE__, __LINE__))
#endif /* NDEBUG */

#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
#undef static_assert
#define static_assert _Static_assert
#endif
