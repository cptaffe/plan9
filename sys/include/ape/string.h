#ifndef __STRING_H_
#define __STRING_H_
#pragma lib "/$M/lib/ape/libap.a"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *memcpy(void *, const void *, size_t);
extern void* memccpy(void*, void*, int, size_t);
extern void *memmove(void *, const void *, size_t);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, size_t);
extern int memcmp(const void *, const void *, size_t);
extern int strcmp(const char *, const char *);
extern int strcoll(const char *, const char *);
extern char* strdup(char*);
extern int strncmp(const char *, const char *, size_t);
extern size_t strxfrm(char *, const char *, size_t);
extern void *memchr(const void *, int, size_t);
extern char *strchr(const char *, int);
extern size_t strcspn(const char *, const char *);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern size_t strspn(const char *, const char *);
extern char *strstr(const char *, const char *);
extern char *strtok(char *, const char *);
extern void *memset(void *, int, size_t);
extern char *strerror(int);
extern size_t strlen(const char *);
#if _POSIX_C_SOURCE >= 200809L || defined(_BSD_EXTENSION)
extern size_t strnlen(const char *, size_t);
extern char *strndup(const char *, size_t);
#endif
#if _POSIX_C_SOURCE >= 202405L || defined(_BSD_EXTENSION)
extern void *memmem(const void *, size_t, const void *, size_t);
extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);
#endif

#if defined(_REENTRANT_SOURCE) || _POSIX_C_SOURCE >= 200112L
extern int strerror_r(int, char *, size_t);
#endif

#ifdef _REENTRANT_SOURCE
extern char *strtok_r(char *, const char *, char **);
#endif

#ifdef _BSD_EXTENSION
#include <bsd.h>
#endif

#ifdef __cplusplus
}
#endif

#endif
