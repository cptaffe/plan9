#ifndef __ENDIAN_H
#define __ENDIAN_H
/*
 * POSIX.1-2024 endian.h for wasm32, which is little-endian.
 */
#include <stdint.h>

#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN	4321
#define BYTE_ORDER	LITTLE_ENDIAN

#define htobe16(x)	((uint16_t)__builtin_bswap16(x))
#define htobe32(x)	((uint32_t)__builtin_bswap32(x))
#define htobe64(x)	((uint64_t)__builtin_bswap64(x))
#define be16toh(x)	((uint16_t)__builtin_bswap16(x))
#define be32toh(x)	((uint32_t)__builtin_bswap32(x))
#define be64toh(x)	((uint64_t)__builtin_bswap64(x))
#define htole16(x)	((uint16_t)(x))
#define htole32(x)	((uint32_t)(x))
#define htole64(x)	((uint64_t)(x))
#define le16toh(x)	((uint16_t)(x))
#define le32toh(x)	((uint32_t)(x))
#define le64toh(x)	((uint64_t)(x))

#endif
