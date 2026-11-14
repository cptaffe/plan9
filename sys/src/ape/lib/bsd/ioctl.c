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
