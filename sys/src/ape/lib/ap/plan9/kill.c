#include "lib.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>

static int
note(int pid, char *msg, char *fmt)
{
	int f;
	char pname[50];

	sprintf(pname, fmt, pid);
	f = open(pname, O_WRONLY);
	if(f < 0){
		errno = ESRCH;
		return -1;
	}
	if(msg != 0 && write(f, msg, strlen(msg)) < 0){
		close(f);
		errno = EPERM;
		return -1;
	}
	close(f);
	return 0;
}

static int
noteid(char *pid)
{
	int f, n;
	char name[50], buf[16];

	snprintf(name, sizeof name, "/proc/%s/noteid", pid);
	if((f = open(name, O_RDONLY)) < 0)
		return -1;
	n = read(f, buf, sizeof buf-1);
	close(f);
	if(n <= 0)
		return -1;
	buf[n] = 0;
	return atoi(buf);
}

static int
group(int g, char *msg)
{
	DIR *d;
	struct dirent *e;
	char name[50];
	int f, perm;

	if((d = opendir("/proc")) == NULL){
		errno = ESRCH;
		return -1;
	}
	perm = 0;
	while((e = readdir(d)) != NULL){
		if(e->d_name[0] < '0' || e->d_name[0] > '9' || noteid(e->d_name) != g)
			continue;
		snprintf(name, sizeof name, "/proc/%s/notepg", e->d_name);
		if((f = open(name, O_WRONLY)) < 0){
			if(errno == EPERM || errno == EACCES)
				perm = 1;
			continue;
		}
		if(msg != 0 && write(f, msg, strlen(msg)) < 0){
			if(errno == EPERM || errno == EACCES)
				perm = 1;
			close(f);
			continue;
		}
		close(f);
		closedir(d);
		if(msg != 0 && getpgrp() == g)
			note(getpid(), msg, "/proc/%d/note");
		return 0;
	}
	closedir(d);
	errno = perm? EPERM: ESRCH;
	return -1;
}

int
kill(pid_t pid, int sig)
{
	char *msg;
	int r;

	if(sig == 0)
		msg = 0;
	else {
		msg = _sigstring(sig);
		if(msg == 0) {
			errno = EINVAL;
			return -1;
		}
	}

	if(pid < 0)
		r = group(-pid, msg);
	else if(pid == 0)
		r = note(getpid(), msg, "/proc/%d/notepg");
	else
		r = note(pid, msg, "/proc/%d/note");
	return r;
}
