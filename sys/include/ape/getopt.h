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

struct option
{
	const char	*name;
	int	has_arg;
	int	*flag;
	int	val;
};

enum
{
	no_argument,
	required_argument,
	optional_argument,
};

extern int	getopt_long(int, char *const*, const char*, const struct option*, int*);
extern int	getopt_long_only(int, char *const*, const char*, const struct option*, int*);

#ifdef __cplusplus
}
#endif

#endif
