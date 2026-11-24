typedef struct Evreader	Evreader;

struct Evreader
{
	Ref	ref;
	Channel	*c;
	Channel	*quit;
	int	fd;
	Winbuf	b;
	Event	e[2];
	Event	x;
};

int	_winopen(Win*, char*, int);
int*	_winfd(Win*, char*);
int	_winguarded(char*);
int	_winfid(Win*, char*);
char*	_winpath(Win*, char*, char*, int);
char*	_winevsmprint(char*, va_list);
int	_winreadev(int, Winbuf*, Event*, Event*);
void	_winevfree(Evreader*);
