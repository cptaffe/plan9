#ifndef __IOCTL_H__
#define __IOCTL_H__

#ifndef _BSD_EXTENSION
    This header file is an extension to ANSI/POSIX
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* FIONREAD: return number of bytes readable in *(long*)arg */
#define FIONREAD 1

/* TIOCGWINSZ: the terminal's size in *(struct winsize*)arg, as vt(1)'s $LINES and $COLS say */
#define TIOCGWINSZ 2

struct winsize {
	unsigned short	ws_row;
	unsigned short	ws_col;
	unsigned short	ws_xpixel;
	unsigned short	ws_ypixel;
};

int ioctl(int, unsigned long, void*);

#ifdef __cplusplus
}
#endif


#endif /* !__IOCTL_H__ */
