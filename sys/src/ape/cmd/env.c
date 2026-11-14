#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

extern char **environ;

static void
usage(void)
{
	fprintf(stderr, "usage: env [-i] [-u name] [name=value ...] [utility [argument ...]]\n");
	exit(125);
}

int
main(int argc, char **argv)
{
	static char *none[] = { NULL };
	char **e, *v;
	int i;

	for(i = 1; i < argc && argv[i][0] == '-'; i++){
		if(strcmp(argv[i], "--") == 0){
			i++;
			break;
		}
		if(strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-") == 0)
			environ = none;
		else if(strcmp(argv[i], "-u") == 0){
			if(++i >= argc)
				usage();
			unsetenv(argv[i]);
		}else
			usage();
	}
	for(; i < argc && (v = strchr(argv[i], '=')) != NULL && v != argv[i]; i++){
		*v++ = 0;
		if(setenv(argv[i], v, 1) < 0){
			fprintf(stderr, "env: %s: %s\n", argv[i], strerror(errno));
			return 125;
		}
	}
	if(i >= argc){
		for(e = environ; *e; e++)
			printf("%s\n", *e);
		return 0;
	}
	execvp(argv[i], argv+i);
	fprintf(stderr, "env: %s: %s\n", argv[i], strerror(errno));
	return errno == ENOENT ? 127 : 126;
}
