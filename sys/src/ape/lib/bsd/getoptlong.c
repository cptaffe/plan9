#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

static int
longopt(int argc, char *const *argv, const char *opts, const struct option *lo, int *idx, char *a)
{
	char *eq;
	int i, n;

	eq = strchr(a, '=');
	n = eq != NULL ? eq-a : strlen(a);
	for(i = 0; lo[i].name != NULL; i++)
		if(strncmp(lo[i].name, a, n) == 0 && strlen(lo[i].name) == n)
			break;
	optind++;
	optarg = NULL;
	if(lo[i].name == NULL){
		if(opterr)
			fprintf(stderr, "%s: unrecognized option '%.*s'\n", argv[0], n, a);
		optopt = 0;
		return '?';
	}
	if(idx != NULL)
		*idx = i;
	if(lo[i].has_arg == no_argument && eq != NULL){
		if(opterr)
			fprintf(stderr, "%s: option '%s' takes no argument\n", argv[0], lo[i].name);
		optopt = lo[i].val;
		return '?';
	}
	if(lo[i].has_arg != no_argument){
		if(eq != NULL)
			optarg = eq+1;
		else if(lo[i].has_arg == required_argument){
			if(optind >= argc){
				if(opterr)
					fprintf(stderr, "%s: option '%s' needs an argument\n", argv[0], lo[i].name);
				optopt = lo[i].val;
				return opts[0] == ':' ? ':' : '?';
			}
			optarg = argv[optind++];
		}
	}
	if(lo[i].flag != NULL){
		*lo[i].flag = lo[i].val;
		return 0;
	}
	return lo[i].val;
}

int
getopt_long(int argc, char *const *argv, const char *opts, const struct option *lo, int *idx)
{
	char *a;

	if(optind < argc && (a = argv[optind])[0] == '-' && a[1] == '-'){
		if(a[2] == 0){
			optind++;
			return -1;
		}
		return longopt(argc, argv, opts, lo, idx, a+2);
	}
	return getopt(argc, argv, opts);
}

int
getopt_long_only(int argc, char *const *argv, const char *opts, const struct option *lo, int *idx)
{
	char *a;
	int i, n;

	if(optind < argc && (a = argv[optind])[0] == '-' && a[1] != '-' && a[1] != 0){
		n = strcspn(a+1, "=");
		for(i = 0; lo[i].name != NULL; i++)
			if(strncmp(lo[i].name, a+1, n) == 0 && strlen(lo[i].name) == n)
				return longopt(argc, argv, opts, lo, idx, a+1);
	}
	return getopt_long(argc, argv, opts, lo, idx);
}
