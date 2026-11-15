#define _BSD_EXTENSION
#include "lib.h"
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>
#include <poll.h>

int
poll(struct pollfd *fds, nfds_t n, int timeout)
{
	fd_set r, w, e;
	struct timeval tv;
	nfds_t i;
	int fd, max, k, ready;

	if(n > OPEN_MAX){
		errno = EINVAL;
		return -1;
	}
	FD_ZERO(&r);
	FD_ZERO(&w);
	FD_ZERO(&e);
	max = 0;
	k = 0;
	for(i = 0; i < n; i++){
		fds[i].revents = 0;
		fd = fds[i].fd;
		if(fd < 0)
			continue;
		if(fd >= OPEN_MAX || !(_fdinfo[fd].flags&FD_ISOPEN)){
			fds[i].revents = POLLNVAL;
			k++;
			continue;
		}
		if(fds[i].events & (POLLIN|POLLRDNORM)){
			FD_SET(fd, &r);
			FD_SET(fd, &e);
		}
		if(fds[i].events & (POLLOUT|POLLWRNORM))
			FD_SET(fd, &w);
		if(fd >= max)
			max = fd+1;
	}
	if(k > 0)
		timeout = 0;
	tv.tv_sec = timeout/1000;
	tv.tv_usec = timeout%1000*1000;
	if(select(max, &r, &w, &e, timeout < 0 ? nil : &tv) < 0)
		return -1;
	ready = 0;
	for(fd = 0; fd < max; fd++)
		if(FD_ISSET(fd, &r)){
			FD_SET(fd, &e);
			ready = 1;
		}
	if(ready){
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if(select(max, nil, nil, &e, &tv) < 0)
			return -1;
	}
	for(i = 0; i < n; i++){
		fd = fds[i].fd;
		if(fd < 0 || fds[i].revents == POLLNVAL)
			continue;
		if(FD_ISSET(fd, &r))
			fds[i].revents |= fds[i].events & (POLLIN|POLLRDNORM);
		if(FD_ISSET(fd, &w))
			fds[i].revents |= fds[i].events & (POLLOUT|POLLWRNORM);
		if(FD_ISSET(fd, &e))
			fds[i].revents |= POLLHUP;
		if(fds[i].revents)
			k++;
	}
	return k;
}
