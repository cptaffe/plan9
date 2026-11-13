/*
 * The shell's process is posix_spawn's, not fork's: on wasm, a program that runs a command
 * needs no asyncify.
 */
#define _POSIX_C_SOURCE 200809L
#include "lib.h"
#include <stdlib.h>
#include <stdio.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int
system(const char *s)
{
	int w, status;
	pid_t pid;
	char cmd[30], *oty, *argv[4];

	oty = getenv("cputype");
	if(!oty)
		return -1;
	if(!s)
		return 1; /* a command interpreter is available */
	snprintf(cmd, sizeof cmd, "/%s/bin/ape/sh", oty);
	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = (char*)s;
	argv[3] = NULL;
	if(posix_spawn(&pid, cmd, NULL, NULL, argv, environ) != 0)
		return -1;
	for(;;) {
		w = wait(&status);
		if(w == -1 || w == pid)
			break;
	}

	if(w == -1){
		_syserrno();
		return w;
	}
	return status;
}
