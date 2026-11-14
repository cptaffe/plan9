#include <signal.h>

const char *const sys_signame[NSIG] = {
	"sig0", "hup", "int", "quit", "ill", "abrt", "fpe", "kill",
	"segv", "pipe", "alrm", "term", "usr1", "usr2", "bus", "chld",
	"cont", "stop", "tstp", "ttin", "ttou", "winch", "trap", "sys",
	"urg", "io", "prof", "vtalrm", "xcpu", "xfsz",
};
