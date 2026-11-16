#ifndef __GETOPT_H
#define __GETOPT_H
#pragma lib "/$M/lib/ape/libbsd.a"

#ifdef __cplusplus
extern "C" {
#endif

extern int	getopt(int, char *const*, const char*);
extern int	opterr;
extern int	optind;
extern int	optopt;
extern char	*optarg;

#ifdef __cplusplus
}
#endif

#endif
