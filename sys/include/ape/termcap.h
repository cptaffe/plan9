#ifndef __TERMCAP_H
#define __TERMCAP_H
#pragma lib "/$M/lib/ape/libtermcap.a"

#ifdef __cplusplus
extern "C" {
#endif

extern char PC;
extern char *BC;
extern char *UP;
extern short ospeed;

extern int	tgetent(char *, const char *);
extern int	tgetflag(const char *);
extern int	tgetnum(const char *);
extern char	*tgetstr(const char *, char **);
extern char	*tgoto(const char *, int, int);
extern int	tputs(const char *, int, int (*)(int));

#ifdef __cplusplus
}
#endif

#endif
