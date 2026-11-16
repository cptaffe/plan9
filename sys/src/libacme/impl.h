extern QLock	_winseqlk;

int	_winopen(Win*, char*, int);
int*	_winfd(Win*, char*);
int	_winguarded(char*);
int	_wfid(Win*, char*);
