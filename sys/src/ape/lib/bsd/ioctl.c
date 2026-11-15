/* posix */
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

/* bsd extensions */
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <termios.h>

static int
pttysize(struct winsize *w)
{
	char buf[128], *p;
	int fd, n, i;

	if((fd = open("/dev/consctl", O_RDONLY)) < 0)
		return -1;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if(n <= 0)
		return -1;
	buf[n] = 0;
	if(strncmp(buf, "IOR ", 4) != 0)
		return -1;
	p = buf;
	for(i = 0; i < 1+4+NCCS+1 && p != NULL; i++)
		if((p = strchr(p, ' ')) != NULL)
			p++;
	if(p == NULL)
		return -1;
	w->ws_col = strtol(p, &p, 10);
	w->ws_row = strtol(p, &p, 10);
	if(w->ws_col <= 0 || w->ws_row <= 0)
		return -1;
	w->ws_xpixel = 0;
	w->ws_ypixel = 0;
	return 0;
}

static int
envint(char *name)
{
	char buf[32];
	int fd, n;

	snprintf(buf, sizeof buf, "/env/%s", name);
	if((fd = open(buf, O_RDONLY)) < 0)
		return 0;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;
	return atoi(buf);
}

int
ioctl(int fd, unsigned long request, void* arg)
{
	struct stat d;
	struct winsize *w;

	if(request == FIONREAD) {
		if(fstat(fd, &d) < 0) {
			errno = EBADF;
			return -1;
		}
		/* this works if the file is buffered somehow */
		*(long*)arg = d.st_size;
		return 0;
	} else if(request == TIOCGWINSZ) {
		w = arg;
		if(isatty(fd) && pttysize(w) == 0)
			return 0;
		if(!isatty(fd) || (w->ws_row = envint("LINES")) == 0 || (w->ws_col = envint("COLS")) == 0) {
			errno = ENOTTY;
			return -1;
		}
		w->ws_xpixel = envint("XPIXELS");
		w->ws_ypixel = envint("YPIXELS");
		return 0;
	} else {
		errno = EINVAL;
		return -1;
	}
}
