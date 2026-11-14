#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

char *
getpass(const char *prompt)
{
	static char buf[256];
	int f, c, n, r;
	char ch;

	if((f = open("/dev/cons", O_RDWR)) < 0)
		return NULL;
	c = open("/dev/consctl", O_WRONLY);
	if(c >= 0)
		write(c, "rawon", 5);
	write(f, prompt, strlen(prompt));
	n = 0;
	while((r = read(f, &ch, 1)) == 1 && ch != '\n' && ch != '\r' && ch != 0x04){
		if(ch == '\b'){
			if(n > 0)
				n--;
		}else if(ch == 0x15)
			n = 0;
		else if(n < sizeof buf - 1)
			buf[n++] = ch;
	}
	buf[n] = 0;
	write(f, "\n", 1);
	if(c >= 0)
		close(c);
	close(f);
	return r < 0 ? NULL : buf;
}
