char mode;			/* '\0', 'e', 'f', 'h', 'u' */
char bflag;			/* ignore multiple and trailing blanks */
char rflag;			/* recurse down directory trees */
char mflag;			/* pseudo flag: doing multiple files, one dir */
int anychange;
extern Biobuf	stdout;
extern int	binary;
extern int	lines;

#define MALLOC(t, n)		((t *)emalloc((n)*sizeof(t)))
#define REALLOC(p, t, n)	((t *)erealloc((void *)(p), (n)*sizeof(t)))
#define FREE(p)			free((void *)(p))

#define MAXPATHLEN	1024

int mkpathname(char *, char *, char *);
void *emalloc(unsigned);
void *erealloc(void *, unsigned);
void diff(char *, char *, int);
void diffdir(char *, char *, int);
void diffreg(char *, char *, char *, char *);
Biobuf *prepare(int, char *, char *);
void panic(int, char *, ...);
void check(Biobuf *, Biobuf *);
int sameline(int, int, int);
int lineindent(int, int);
void slide(void);
void change(int, int, int, int);
void flushchanges(void);
